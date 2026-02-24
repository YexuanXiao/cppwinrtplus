#include "pch.h"
#include <algorithm>
#include <ctime>
#include <iterator>
#include "strings.h"
#include "settings.h"
#include "type_writers.h"
#include "helpers.h"
#include "code_writers.h"
#include "component_writers.h"
#include "file_writers.h"
#include "type_writers.h"

namespace cppwinrt
{
    settings_type settings;

    struct usage_exception {};

    static constexpr option options[]
    {
        { "input", 0, option::no_max, "<spec>", "Windows metadata to include in projection" },
        { "reference", 0, option::no_max, "<spec>", "Windows metadata to reference from projection" },
        { "output", 0, 1, "<path>", "Location of generated projection and component templates" },
        { "component", 0, 1, "[<path>]", "Generate component templates, and optional implementation" },
        { "name", 0, 1, "<name>", "Specify explicit name for component files" },
        { "verbose", 0, 0, {}, "Show detailed progress information" },
        { "overwrite", 0, 0, {}, "Overwrite generated component files" },
        { "prefix", 0, 0, {}, "Use dotted namespace convention for component files (defaults to folders)" },
        { "pch", 0, 1, "<name>", "Specify name of precompiled header file (defaults to pch.h; use '.' to disable)" },
        { "config", 0, 1, "<path>", "Read include/exclude prefixes from config file" },
        { "include", 0, option::no_max, "<prefix>", "One or more prefixes to include in input" },
        { "exclude", 0, option::no_max, "<prefix>", "One or more prefixes to exclude from input" },
        { "base", 0, 0, {}, "Generate base.h unconditionally" },
        { "modules", 0, 0, {}, "Generate namespace modules; disables winrt.ixx and PCH" },
        { "optimize", 0, 0, {}, "Generate component projection with unified construction support" },
        { "help", 0, option::no_max, {}, "Show detailed help with examples" },
        { "?", 0, option::no_max, {}, {} },
        { "library", 0, 1, "<prefix>", "Specify library prefix (defaults to winrt)" },
        { "filter" }, // One or more prefixes to include in input (same as -include)
        { "license", 0, 1, "[<path>]", "Generate license comment from template file" },
        { "brackets", 0, 0 }, // Use angle brackets for #includes (defaults to quotes)
        { "fastabi", 0, 0 }, // Enable support for the Fast ABI
        { "ignore_velocity", 0, 0 }, // Ignore feature staging metadata and always include implementations
        { "synchronous", 0, 0 }, // Instructs cppwinrt to run on a single thread to avoid file system issues in batch builds
    };

    static void print_usage(writer& w)
    {
        static auto printColumns = [](writer& w, std::string_view const& col1, std::string_view const& col2)
        {
            w.write_printf("  %-20s%s\n", col1.data(), col2.data());
        };

        static auto printOption = [](writer& w, option const& opt)
        {
            if(opt.desc.empty())
            {
                return;
            }
            printColumns(w, w.write_temp("-% %", opt.name, opt.arg), opt.desc);
        };

        auto format = R"(
C++/WinRT Plus v%
Copyright (c) Microsoft Corporation. All rights reserved.
Copyright (c) 2026 YexuanXiao and the C++/WinRT Plus Project. All rights reserved.

  cppwinrt.exe [options...]

Options:

%  ^@<path>             Response file containing command line options

Where <spec> is one or more of:

  path                Path to winmd file or recursively scanned folder
)"
#if defined(_WIN32) || defined(_WIN64)
R"(  local               Local ^%WinDir^%\System32\WinMetadata folder
  sdk[+]              Current version of Windows SDK [with extensions]
  10.0.12345.0[+]     Specific version of Windows SDK [with extensions]
)"
#endif
        ;
        w.write(format, CPPWINRT_VERSION_STRING, bind_each(printOption, options));
    }

    static bool is_config_whitespace(wchar_t const c) noexcept
    {
        return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
    }

    static std::string utf8_from_utf16(wchar_t const* value, std::uint32_t length)
    {
        if (!value || length == 0)
        {
            return {};
        }

        // A valid CppWinRT.config only contains ASCII in <prefix> element text. For ASCII, UTF-16 code units map
        // directly to UTF-8 bytes, so we can validate and truncate without transcoding.
        auto const* first = std::find_if_not(value, value + length, is_config_whitespace);
        auto const* last = std::find_if_not(
            std::make_reverse_iterator(value + length),
            std::make_reverse_iterator(first),
            is_config_whitespace).base();

        std::string result;
        result.reserve(last - first);
        for (auto const* current = first; current != last; ++current)
        {
            auto const ch = *current;
            if (ch > 0x7F)
            {
                throw_invalid("Malformed CppWinRT.config: <prefix> values must be ASCII");
            }
            result.push_back(static_cast<char>(ch));
        }
        return result;
    }

    static void read_config_file(std::filesystem::path const& path)
    {
#if defined(_WIN32) || defined(_WIN64)

        xml_input input;
        const char* purpose = "CppWinRT.config";
        if (!open_xml_input(path, xml_requirement::optional, purpose, input))
        {
            return;
        }

        XmlNodeType node_type = XmlNodeType_None;
        bool in_include{};
        bool in_exclude{};
        bool saw_configuration{};

        while (S_OK == input.reader->Read(&node_type))
        {
            if (node_type == XmlNodeType_Element)
            {
                wchar_t const* name{ nullptr };
                check_xml(input.reader->GetLocalName(&name, nullptr), purpose);

                if (0 == wcscmp(name, L"configuration"))
                {
                    saw_configuration = true;
                    continue;
                }

                if (0 == wcscmp(name, L"include"))
                {
                    in_include = true;
                    in_exclude = false;
                    continue;
                }

                if (0 == wcscmp(name, L"exclude"))
                {
                    in_exclude = true;
                    in_include = false;
                    continue;
                }

                if (0 != wcscmp(name, L"prefix") || (!in_include && !in_exclude))
                {
                    continue;
                }

                if (input.reader->IsEmptyElement())
                {
                    continue;
                }

                XmlNodeType content_type = XmlNodeType_None;
                if (S_OK != input.reader->Read(&content_type))
                {
                    break;
                }

                while (content_type == XmlNodeType_Whitespace)
                {
                    if (S_OK != input.reader->Read(&content_type))
                    {
                        break;
                    }
                }

                if (content_type == XmlNodeType_Text || content_type == XmlNodeType_CDATA)
                {
                    wchar_t const* text{ nullptr };
                    std::uint32_t length{};
                    check_xml(input.reader->GetValue(&text, &length), purpose);

                    auto const trimmed = utf8_from_utf16(text, length);
                    if (!trimmed.empty())
                    {
                        if (in_include)
                        {
                            settings.include.insert(trimmed);
                        }
                        else
                        {
                            settings.exclude.insert(trimmed);
                        }
                    }
                }
            }
            else if (node_type == XmlNodeType_EndElement)
            {
                wchar_t const* name{ nullptr };
                check_xml(input.reader->GetLocalName(&name, nullptr), purpose);

                if (0 == wcscmp(name, L"include"))
                {
                    in_include = false;
                }
                else if (0 == wcscmp(name, L"exclude"))
                {
                    in_exclude = false;
                }
            }
        }

        if (!saw_configuration)
        {
            throw_invalid("Malformed CppWinRT.config file: missing <configuration> root element");
        }
#else
        (void)path;
        throw_invalid("Option '-config' is only supported on Windows");
#endif
    }

    static void process_args(reader const& args)
    {
        settings.verbose = args.exists("verbose");
        settings.fastabi = args.exists("fastabi");
        settings.modules = args.exists("modules");

        settings.input = args.files("input", database::is_database);
        settings.reference = args.files("reference", database::is_database);

        settings.component = args.exists("component");
        settings.base = args.exists("base");

        settings.license = args.exists("license");
        settings.brackets = args.exists("brackets");

        path output_folder = args.value("output", ".");
        create_directories(output_folder / "winrt/impl");
        settings.output_folder = canonical(output_folder).string();
        settings.output_folder += std::filesystem::path::preferred_separator;

        if (args.exists("config"))
        {
            std::string config_file = args.value("config");
            if (config_file.empty())
            {
                throw_invalid("Option 'config' requires exactly 1 value");
            }

            read_config_file(config_file);
        }

        for (auto && include : args.values("include"))
        {
            settings.include.insert(include);
        }

        for (auto && include : args.values("filter"))
        {
            settings.include.insert(include);
        }

        for (auto && exclude : args.values("exclude"))
        {
            settings.exclude.insert(exclude);
        }

        if (settings.license)
        {
            std::string license_arg = args.value("license");
            if (license_arg.empty())
            {
                settings.license_template = R"(// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
)";
            }
            else
            {
                std::filesystem::path template_path{ license_arg };
                std::ifstream template_file(absolute(template_path));
                if (template_file.fail())
                {
                    throw_invalid("Cannot read license template file '", absolute(template_path).string() + "'");
                }
                std::string line_buf;
                while (getline(template_file, line_buf))
                {
                    if (line_buf.empty())
                    {
                        settings.license_template += "//\n";
                    }
                    else
                    {
                        settings.license_template += "// ";
                        settings.license_template += line_buf;
                        settings.license_template += "\n";
                    }
                }
            }
        }

        if (settings.component)
        {
            settings.component_overwrite = args.exists("overwrite");
            settings.component_name = args.value("name");

            if (settings.component_name.empty())
            {
                // For compatibility with C++/WinRT 1.0, the component_name defaults to the *first*
                // input, hence the use of values() here that will return the args in input order.

                auto& values = args.values("input");

                if (!values.empty())
                {
                    settings.component_name = path(values[0]).filename().replace_extension().string();
                }
            }

            settings.component_pch = args.value("pch", "pch.h");
            settings.component_prefix = args.exists("prefix");
            settings.component_lib = args.value("library", "winrt");
            settings.component_opt = args.exists("optimize");
            settings.component_ignore_velocity = args.exists("ignore_velocity");

            if (settings.component_pch == ".")
            {
                settings.component_pch.clear();
            }

            if (settings.modules)
            {
                settings.component_pch.clear();
            }

            auto component = args.value("component");

            if (!component.empty())
            {
                create_directories(component);
                settings.component_folder = canonical(component).string();
                settings.component_folder += std::filesystem::path::preferred_separator;
            }
        }
    }

    static auto get_files_to_cache()
    {
        std::vector<std::string> files;
        files.insert(files.end(), settings.input.begin(), settings.input.end());
        files.insert(files.end(), settings.reference.begin(), settings.reference.end());
        return files;
    }

    static void build_filters(cache const& c)
    {
        std::set<std::string> include_prefixes;
        if (settings.include.empty())
        {
            include_prefixes.insert("");
        }
        else
        {
            include_prefixes = settings.include;
        }

        if (settings.reference.empty())
        {
            if (settings.include.empty() && settings.exclude.empty())
            {
                return;
            }

            settings.projection_filter = { include_prefixes, settings.exclude };
            settings.component_filter = { include_prefixes, settings.exclude };
            return;
        }

        std::set<std::string> include;

        for (auto file : settings.input)
        {
            auto db = std::find_if(c.databases().begin(), c.databases().end(), [&](auto&& db)
            {
                return db.path() == file;
            });

            for (auto&& type : db->TypeDef)
            {
                if (!type.Flags().WindowsRuntime())
                {
                    continue;
                }

                std::string full_name{ type.TypeNamespace() };
                full_name += '.';
                full_name += type.TypeName();
                include.insert(full_name);
            }
        }

        winmd::reader::filter prefix_filter{ include_prefixes, settings.exclude };
        std::set<std::string> filtered;
        for (auto&& type : include)
        {
            if (prefix_filter.includes(type))
            {
                filtered.insert(type);
            }
        }

        settings.projection_filter = { filtered, {} };
        settings.component_filter = { filtered, {} };
    }

    static void build_fastabi_cache(cache const& c)
    {
        if (!settings.fastabi)
        {
            return;
        }

        for (auto&& [ns, members] : c.namespaces())
        {
            for (auto&& type : members.classes)
            {
                if (!has_fastabi(type))
                {
                    continue;
                }

                auto default_interface = get_default_interface(type);

                if (default_interface.type() == TypeDefOrRef::TypeDef)
                {
                    settings.fastabi_cache.try_emplace(default_interface.TypeDef(), type);
                }
                else
                {
                    settings.fastabi_cache.try_emplace(find_required(default_interface.TypeRef()), type);
                }
            }
        }
    }

    static void remove_foundation_types(cache& c)
    {
        c.remove_type("Windows.Foundation", "DateTime");
        c.remove_type("Windows.Foundation", "EventRegistrationToken");
        c.remove_type("Windows.Foundation", "HResult");
        c.remove_type("Windows.Foundation", "Point");
        c.remove_type("Windows.Foundation", "Rect");
        c.remove_type("Windows.Foundation", "Size");
        c.remove_type("Windows.Foundation", "TimeSpan");

        c.remove_type("Windows.Foundation.Numerics", "Matrix3x2");
        c.remove_type("Windows.Foundation.Numerics", "Matrix4x4");
        c.remove_type("Windows.Foundation.Numerics", "Plane");
        c.remove_type("Windows.Foundation.Numerics", "Quaternion");
        c.remove_type("Windows.Foundation.Numerics", "Vector2");
        c.remove_type("Windows.Foundation.Numerics", "Vector3");
        c.remove_type("Windows.Foundation.Numerics", "Vector4");
    }

    [[nodiscard]] static std::vector<std::vector<std::string>> compute_strongly_connected_components(std::map<std::string, std::vector<std::string>> const& graph)
    {
        std::vector<std::string> nodes;
        nodes.reserve(graph.size());

        for (auto&& [node, _] : graph)
        {
            nodes.push_back(node);
        }

        std::unordered_map<std::string, int> index;
        std::unordered_map<std::string, int> lowlink;
        std::unordered_map<std::string, bool> on_stack;
        std::vector<std::string> stack;
        int next_index{};
        std::vector<std::vector<std::string>> components;

        std::function<void(std::string const&)> strongconnect = [&](std::string const& node)
        {
            index.emplace(node, next_index);
            lowlink.emplace(node, next_index);
            ++next_index;

            stack.push_back(node);
            on_stack[node] = true;

            auto const& deps = graph.at(node);

            for (auto&& dep : deps)
            {
                if (graph.find(dep) == graph.end())
                {
                    continue;
                }

                if (index.find(dep) == index.end())
                {
                    strongconnect(dep);
                    lowlink[node] = (std::min)(lowlink[node], lowlink[dep]);
                }
                else if (on_stack[dep])
                {
                    lowlink[node] = (std::min)(lowlink[node], index[dep]);
                }
            }

            if (lowlink[node] != index[node])
            {
                return;
            }

            std::vector<std::string> component;

            while (true)
            {
                auto current = std::move(stack.back());
                stack.pop_back();
                on_stack[current] = false;
                component.push_back(std::move(current));

                if (component.back() == node)
                {
                    break;
                }
            }

            components.push_back(std::move(component));
        };

        for (auto&& node : nodes)
        {
            if (index.find(node) == index.end())
            {
                strongconnect(node);
            }
        }

        return components;
    }

    static int run(int const argc, char** argv)
    {
        int result{};
        writer w;

        try
        {
            auto start = get_start_time();

            reader args{ argc, argv, options };

            if (!args || args.exists("help") || args.exists("?"))
            {
                throw usage_exception{};
            }

            process_args(args);
            cache c{ get_files_to_cache(), [](TypeDef const& type) { return type.Flags().WindowsRuntime(); } };
            remove_foundation_types(c);
            build_filters(c);
            settings.base = settings.base || settings.reference.empty();
            settings.base = settings.base || settings.modules;
            build_fastabi_cache(c);

            if (settings.verbose)
            {
                {
                    char* path = argv[0];
#if defined(_WIN32) || defined(_WIN64)
                    char path_buf[32768];
                    DWORD path_size = GetModuleFileNameA(nullptr, path_buf, sizeof(path_buf));
                    if (path_size)
                    {
                        path_buf[sizeof(path_buf) - 1] = 0;
                        path = path_buf;
                    }
#endif
                    w.write(" tool:  %\n", path);
                }
                w.write(" ver:   %\n", CPPWINRT_VERSION_STRING);

                for (auto&& file : settings.input)
                {
                    w.write(" in:    %\n", file);
                }

                for (auto&& file : settings.reference)
                {
                    w.write(" ref:   %\n", file);
                }

                w.write(" out:   %\n", settings.output_folder);

                if (!settings.component_folder.empty())
                {
                    w.write(" cout:  %\n", settings.component_folder);
                }
            }

            w.flush_to_console();
            task_group group;
            group.synchronous(args.exists("synchronous"));
            std::map<std::string, std::vector<std::string>> module_imports;
            writer ixx;
            if (!settings.modules)
            {
                write_preamble(ixx);
                ixx.write("module;\n");
                ixx.write(strings::base_includes);
                ixx.write("\nexport module winrt;\n#define WINRT_EXPORT export\n\n");
            }
            else
            {
                write_numerics_ixx();
                write_base_ixx();
            }

            for (auto&&[ns, members] : c.namespaces())
            {
                if (!has_projected_types(members) || !settings.projection_filter.includes(members))
                {
                    continue;
                }

                if (!settings.modules)
                {
                    ixx.write("#include \"winrt/%.h\"\n", ns);
                }

                group.add([&, &ns = ns, &members = members]
                {
                    if (settings.modules)
                    {
                        std::vector<std::string> imports;
                        std::set<std::string> combined;
                        write_namespace_0_h(c, ns, members, imports);
                        combined.insert(imports.begin(), imports.end());
                        write_namespace_1_h(c, ns, members, imports);
                        combined.insert(imports.begin(), imports.end());
                        write_namespace_2_h(c, ns, members, imports);
                        combined.insert(imports.begin(), imports.end());
                        write_namespace_h(c, ns, members, imports);
                        combined.insert(imports.begin(), imports.end());

                        module_imports.emplace(ns, std::vector<std::string>{ combined.begin(), combined.end() });
                    }
                    else
                    {
                        std::vector<std::string> unused;
                        write_namespace_0_h(c, ns, members, unused);
                        write_namespace_1_h(c, ns, members, unused);
                        write_namespace_2_h(c, ns, members, unused);
                        write_namespace_h(c, ns, members, unused);
                    }
                });
            }

            if (settings.base)
            {
                if (settings.modules)
                {
                    write_module_h();
                }
                write_base_h();
                if (!settings.modules)
                {
                    ixx.flush_to_file(settings.output_folder + "winrt/winrt.ixx");
                }
            }

            if (settings.component)
            {
                std::vector<TypeDef> classes;

                for (auto&&[ns, members] : c.namespaces())
                {
                    for (auto&& type : members.classes)
                    {
                        if (settings.component_filter.includes(type))
                        {
                            classes.push_back(type);
                        }
                    }
                }

                if (!classes.empty())
                {
                    write_fast_forward_h(classes);
                    write_module_g_cpp(classes);

                    for (auto&& type : classes)
                    {
                        write_component_g_h(type);
                        write_component_g_cpp(type);
                        write_component_h(type);
                        write_component_cpp(type);
                    }
                }
            }

            group.get();

            if (settings.modules)
            {
                auto components = compute_strongly_connected_components(module_imports);
                std::map<std::string, std::vector<std::string>> members_by_owner;
                std::map<std::string, std::string> owner_of;

                for (auto& component : components)
                {
                    std::sort(component.begin(), component.end());
                    auto const& owner = component.front();
                    members_by_owner.emplace(owner, component);

                    for (auto const& ns : component)
                    {
                        owner_of.emplace(ns, owner);
                    }
                }

                for (auto const& [owner, members] : members_by_owner)
                {
                    if (members.size() == 1)
                    {
                        write_namespace_ixx(owner, module_imports.at(owner));
                        continue;
                    }

                    std::set<std::string> external_imports;

                    for (auto const& ns : members)
                    {
                        for (auto const& dep : module_imports.at(ns))
                        {
                            auto found = owner_of.find(dep);

                            if (found != owner_of.end() && found->second == owner)
                            {
                                continue;
                            }

                            external_imports.emplace(dep);
                        }
                    }

                    std::vector<std::string> imports{ external_imports.begin(), external_imports.end() };
                    write_namespace_scc_owner_ixx(c, owner, members, imports);

                    for (auto const& ns : members)
                    {
                        if (ns != owner)
                        {
                            write_namespace_reexport_ixx(ns, owner);
                        }
                    }
                }
            }

            if (settings.verbose)
            {
                w.write(" time:  %ms\n", get_elapsed_time(start));
            }
        }
        catch (usage_exception const&)
        {
            print_usage(w);
        }
        catch (std::exception const& e)
        {
            w.write("cppwinrt : error %\n", e.what());
            result = 1;
        }

        w.flush_to_console(result == 0);
        return result;
    }
}

int main(int const argc, char** argv)
{
    return cppwinrt::run(argc, argv);
}
