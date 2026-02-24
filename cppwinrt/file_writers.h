#pragma once

namespace cppwinrt
{
    // Computes the set of namespace-modules that a given namespace's generated headers depend on.
    // c: metadata cache, used to validate that a dependency is a projected namespace
    // ns: the namespace currently being generated
    // w: a writer after it has populated w.depends while emitting header content
    // imports: populated with a sorted list of projected namespace strings, used by main.cpp to build the
    // module import graph
    static void get_namespace_module_imports(cache const& c, std::string_view const& ns, writer const& w, std::vector<std::string>& imports)
    {
        // Computes the module import list for a namespace based on writer.depends.
        // w.depends: namespaces referenced while generating the header body (via writer::add_depends)
        // Output: unique list of dependent namespaces which are projected (have any projected types)
        // This list drives module generation in `main.cpp`:
        // 1. union dependencies from impl headers and the projection header,
        // 2. compute SCCs to break cycles,
        // 3. emit import ns; for each dependency in the module interface unit.
        imports.clear();

        for (auto&& depends : w.depends)
        {
            if (depends.first == ns)
            {
                continue;
            }

            auto found = c.namespaces().find(depends.first);

            if (found != c.namespaces().end() && has_projected_types(found->second))
            {
                // w.depends is a sorted map, so imports remains sorted and duplicate-free.
                imports.emplace_back(depends.first);
            }
        }
    }

    static void write_base_h()
    {
        writer w;
        write_preamble(w);
        w.write(strings::base_version_odr, CPPWINRT_VERSION_STRING);
        {
            auto wrap_file_guard = wrap_open_file_guard(w, "BASE");
            auto wrap_export_macro_guard = wrap_module_aware_export_macro_guard(w, settings.modules);

            {
                // In module builds, generated projection headers must be "module-aware":
                // When `WINRT_MODULE` is defined (inside a module interface unit), suppress textual includes so the
                // module global fragment can control which headers are included.
                // Switch WINRT_EXPORT between empty (header mode) and `export` (module mode).
                auto wrap_includes_guard = wrap_module_aware_includes_guard(w, settings.modules);
                w.write(strings::base_includes);
            }

            w.write(strings::base_macros);
            w.write(strings::base_types);
            w.write(strings::base_extern);
            w.write(strings::base_source_location);
            w.write(strings::base_meta);
            w.write(strings::base_identity);
            w.write(strings::base_handle);
            w.write(strings::base_lock);
            w.write(strings::base_abi);
            w.write(strings::base_windows);
            w.write(strings::base_com_ptr);
            w.write(strings::base_string);
            w.write(strings::base_string_input);
            w.write(strings::base_string_operators);
            w.write(strings::base_array);
            w.write(strings::base_weak_ref);
            w.write(strings::base_agile_ref);
            w.write(strings::base_error);
            w.write(strings::base_marshaler);
            w.write(strings::base_delegate);
            w.write(strings::base_events);
            w.write(strings::base_activation);
            w.write(strings::base_implements);
            w.write(strings::base_composable);
            w.write(strings::base_foundation);
            w.write(strings::base_chrono);
            w.write(strings::base_security);
            w.write(strings::base_std_hash);
            w.write(strings::base_iterator);
            w.write(strings::base_coroutine_threadpool);
            w.write(strings::base_natvis);
            w.write(strings::base_version);
        }
        w.flush_to_file(settings.output_folder + "winrt/base.h");
    }

    static void write_module_h()
    {
        // Emits winrt/module.h for module builds.
        // module imports do not propagate preprocessor macros, but the C++/WinRT projection relies on a
        // number of WINRT_IMPL_* macros in generated headers. Each module interface unit includes this header in its
        // global module fragment so macros are available consistently during compilation.
        writer w;
        write_preamble(w);
        {
            auto wrap_file_guard = wrap_open_file_guard(w, "MODULE");
            w.write(strings::base_macros);
        }
        w.flush_to_file(settings.output_folder + "winrt/module.h");
    }

    static void write_fast_forward_h(std::vector<TypeDef> const& classes)
    {
        writer w;
        write_preamble(w);
        {
            auto wrap_file_guard = wrap_open_file_guard(w, "FAST_FORWARD");

            w.write(R"(// Transition: compatibility
#ifndef WINRT_MODULE

#ifndef WINRT_EXPORT
#define WINRT_EXPORT
#endif

#endif
)");

            auto const fast_abi_size = get_fastabi_size(w, classes);

            w.write(strings::base_fast_forward,
                fast_abi_size,
                fast_abi_size,
                bind<write_component_fast_abi_thunk>(),
                bind<write_component_fast_abi_vtable>());

        }
        w.flush_to_file(settings.output_folder + "winrt/fast_forward.h");
    }

    static void write_namespace_0_h(cache const& c, std::string_view const& ns, cache::namespace_members const& members, std::vector<std::string>& module_imports)
    {
        // Emits $(out)\winrt\impl\<ns>.0.h.
        // When settings.modules is enabled, also populates module_imports with the set of dependent namespaces
        // found while writing the header body. main.cpp unions the dependency sets from *.0/*.1/*.2/<ns>.h to
        // build a module import graph.
        writer w;
        w.type_namespace = ns;

        {
            auto wrap_type = wrap_type_namespace(w, ns);
            w.write_each<write_enum>(members.enums);
            w.write_each<write_forward>(members.interfaces);
            w.write_each<write_forward>(members.classes);
            w.write_each<write_forward>(members.structs);
            w.write_each<write_forward>(members.delegates);
            w.write_each<write_forward>(members.contracts);
        }
        {
            auto wrap_impl = wrap_impl_namespace(w);
            w.write_each<write_category>(members.interfaces, "interface_category");
            w.write_each<write_category>(members.classes, "class_category");
            w.write_each<write_category>(members.enums, "enum_category");
            w.write_each<write_struct_category>(members.structs);
            w.write_each<write_category>(members.delegates, "delegate_category");

            // Class names are always required for activation.
            // Class, enum, and struct names are required for producing GUIDs for generic types.
            // Interface and delegates names are required for Xaml compatibility.
            // Contract names are used by IsApiContractPresent.
            w.write_each<write_name>(members.classes);
            w.write_each<write_name>(members.enums);
            w.write_each<write_name>(members.structs);
            w.write_each<write_name>(members.interfaces);
            w.write_each<write_name>(members.delegates);
            w.write_each<write_name>(members.contracts);

            w.write_each<write_guid>(members.interfaces);
            w.write_each<write_guid>(members.delegates);
            w.write_each<write_default_interface>(members.classes);
            w.write_each<write_interface_abi>(members.interfaces);
            w.write_each<write_delegate_abi>(members.delegates);
            w.write_each<write_consume>(members.interfaces);
            w.write_each<write_struct_abi>(members.structs);
        }

        if (settings.modules)
        {
            get_namespace_module_imports(c, ns, w, module_imports);
            write_module_aware_export_macro_pop(w);
        }
        else
        {
            module_imports.clear();
        }
        write_close_file_guard(w);
        w.swap();
        write_preamble(w);
        write_open_file_guard(w, ns, '0');

        if (settings.modules)
        {
            write_module_aware_export_macro_push(w);
            write_module_aware_export_includes_start(w);
        }

        for (auto&& depends : w.depends)
        {
            auto wrap_type = wrap_type_namespace(w, depends.first);
            w.write_each<write_forward>(depends.second);
        }

        if (settings.modules)
        {
            write_module_aware_export_includes_end(w);
        }

        w.save_header('0');
    }

    static void write_namespace_1_h(cache const& c, std::string_view const& ns, cache::namespace_members const& members, std::vector<std::string>& module_imports)
    {
        // Emits $(out)\winrt\impl\<ns>.1.h.
        // Populates module_imports when settings.modules is enabled. See write_namespace_0_h.
        writer w;
        w.type_namespace = ns;

        {
            auto wrap_type = wrap_type_namespace(w, ns);
            w.write_each<write_interface>(members.interfaces);
        }
        write_namespace_special_1(w, ns);

        if (settings.modules)
        {
            get_namespace_module_imports(c, ns, w, module_imports);
            write_module_aware_export_macro_pop(w);
        }
        else
        {
            module_imports.clear();
        }

        write_close_file_guard(w);
        w.swap();
        write_preamble(w);
        write_open_file_guard(w, ns, '1');

        if (settings.modules)
        {
            write_module_aware_export_macro_push(w);
            write_module_aware_export_includes_start(w);
        }

        for (auto&& depends : w.depends)
        {
            w.write_depends(depends.first, '0');
        }

        w.write_depends(w.type_namespace, '0');

        if (settings.modules)
        {
            write_module_aware_export_includes_end(w);
        }

        w.save_header('1');
    }

    static void write_namespace_2_h(cache const& c, std::string_view const& ns, cache::namespace_members const& members, std::vector<std::string>& module_imports)
    {
        // Emits $(out)\winrt\impl\<ns>.2.h
        // Populates module_imports when settings.modules is enabled. See write_namespace_0_h.
        writer w;
        w.type_namespace = ns;

        bool promote;
        {
            auto wrap_type = wrap_type_namespace(w, ns);
            w.write_each<write_delegate>(members.delegates);
            promote = write_structs(w, members.structs);
            w.write_each<write_class>(members.classes);
            w.write_each<write_interface_override>(members.classes);
        }

        if (settings.modules)
        {
            get_namespace_module_imports(c, ns, w, module_imports);
            write_module_aware_export_macro_pop(w);
        }
        else
        {
            module_imports.clear();
        }

        write_close_file_guard(w);
        w.swap();
        write_preamble(w);
        write_open_file_guard(w, ns, '2');

        char const impl = promote ? '2' : '1';

        if (settings.modules)
        {
            write_module_aware_export_macro_push(w);
            write_module_aware_export_includes_start(w);
        }

        for (auto&& depends : w.depends)
        {
            w.write_depends(depends.first, impl);
        }

        w.write_depends(w.type_namespace, '1');

        if (settings.modules)
        {
            write_module_aware_export_includes_end(w);
        }

        w.save_header('2');
    }

    static void write_module_global_fragment(writer& w)
    {
        // Common global module fragment used for all generated module interface units.
        // Define WINRT_MODULE so that generated projection headers become module-aware
        // (no dependent includes, WINRT_EXPORT => export).
        // Provide minimal textual includes required for macros / intrinsics / feature-test macros.
        // In Debug builds, include <crtdbg.h> to provide _ASSERTE for WINRT_ASSERT.
        // Provide winrt/module.h to define the WINRT_IMPL_* macros (macros are not shared via import).
        w.write(R"(
module;

#define WINRT_MODULE
#include <intrin.h>
#include <cstddef>
#include <version>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#include "winrt/module.h"

)");
    }

    static void write_base_ixx()
    {
        // Emits $(out)\winrt\winrt.base.ixx  (export module winrt.base;)
        // Export module winrt.base and import std; (header includes are suppressed under WINRT_MODULE).
        // Export `winrt.numerics` so consumers can rely on import winrt.base.
        // Include winrt/base.h, which exports its declarations via WINRT_EXPORT.
        writer w;
        write_preamble(w);
        write_module_global_fragment(w);

        w.write(R"(
export module winrt.base;

// Module dependencies:
//   - std
//   - winrt.numerics (re-exported when available)

import std;
export import winrt.numerics;

#if __has_include(<windowsnumerics.impl.h>)
#define WINRT_IMPL_NUMERICS
#endif

#include "winrt/base.h"
)");

        w.flush_to_file(settings.output_folder + "winrt/winrt.base.ixx");
    }

    static void write_numerics_ixx()
    {
        // Emits $(out)\winrt\winrt.numerics.ixx  (export module winrt.numerics;)
        // <windowsnumerics.impl.h> pulls in large, legacy headers that are hard to control and can trigger module
        // diagnostics when textually included in a module purview.
        // If the header does not exist, then the module exports nothing. To speed up module scanning, modules
        // can't be controlled by preprocessor directives. Therefore, winrt.base cannot conditionally import it.
        // In header builds we preserve the historical behavior (base headers include it), but in module builds we
        // centralize it in this single module and have `winrt.base` re-export it.
        // MSVC warns (C5244) when encountering textual includes inside a module purview; suppress for this file.
        writer w;
        write_preamble(w);
        write_module_global_fragment(w);

        w.write(R"(
export module winrt.numerics;

// Module dependencies:
//   - (none)

#if __has_include(<windowsnumerics.impl.h>)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5244)
#endif
#include <directxmath.h>

#define _WINDOWS_NUMERICS_NAMESPACE_ winrt::Windows::Foundation::Numerics
#define _WINDOWS_NUMERICS_BEGIN_NAMESPACE_ export namespace winrt::Windows::Foundation::Numerics
#define _WINDOWS_NUMERICS_END_NAMESPACE_
#include <windowsnumerics.impl.h>
#undef _WINDOWS_NUMERICS_NAMESPACE_
#undef _WINDOWS_NUMERICS_BEGIN_NAMESPACE_
#undef _WINDOWS_NUMERICS_END_NAMESPACE_
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif
)");

        w.flush_to_file(settings.output_folder + "winrt/winrt.numerics.ixx");
    }

    static void write_namespace_ixx(std::string_view const& ns, std::vector<std::string> const& imports)
    {
        // Emits a per-namespace module interface unit when the namespace is not part of a dependency cycle.
        // ns: namespace name (e.g. "Windows.Foundation")
        // imports: namespace modules to import before including headers
        // Output $(out)\winrt\<ns>.ixx  (export module ns;)
        // Write the common global module fragment (WINRT_MODULE, minimal includes, macros).
        // Export module ns and import std.
        // export import winrt.base; so all namespace modules see the base definitions.
        // import dep for each dependent namespace module (projection headers suppress dependent includes).
        // Include the impl headers (*.0/*.1/*.2) then the projection header (<ns>.h). The headers themselves use
        // WINRT_EXPORT (exported in module builds) for their declarations.
        writer w;
        write_preamble(w);
        write_module_global_fragment(w);

        w.write("export module %;\n\n", ns);

        w.write("// Module dependencies:\n");
        w.write("//   - std\n");
        w.write("//   - winrt.base (re-exported)\n");

        if (imports.empty())
        {
            w.write("//   - (no additional namespace imports)\n");
        }
        else
        {
            for (auto&& module : imports)
            {
                w.write("//   - %\n", module);
            }
        }

        w.write(R"(
// Namespace imports (below) are computed from type references in generated headers.
)");

        w.write(R"(
import std;
export import winrt.base;
)");

        for (auto&& module : imports)
        {
            w.write("import %;\n", module);
        }

        w.write('\n');
        w.write(R"(#include "winrt/impl/%.0.h"
#include "winrt/impl/%.1.h"
#include "winrt/impl/%.2.h"
#include "winrt/%.h"
)",
            ns, ns, ns, ns);

        auto filename = settings.output_folder + "winrt/";
        filename += ns;
        filename += ".ixx";
        w.flush_to_file(filename);
    }

    static void write_namespace_reexport_ixx(std::string_view const& ns, std::string_view const& target)
    {
        // Emits a thin re-export wrapper module.
        // Used when ns is part of a strongly-connected component (cycle) whose declarations are provided by an SCC
        // owner module. This keeps import ns working even when the implementation is consolidated.
        // Output - $(out)\winrt\<ns>.ixx  (export module <ns>; export import <target>;)
        writer w;
        write_preamble(w);

        w.write(R"(// NOTE: This module does not define declarations of its own.
// It re-exports all declarations from the '%' module. This is used to break cycles in the
// WinRT namespace module dependency graph (SCC owner consolidation).
//
// Module dependencies:
//   - % (re-exported)

export module %;
export import %;
)",
            target,
            target,
            ns,
            target);

        auto filename = settings.output_folder + "winrt/";
        filename += ns;
        filename += ".ixx";
        w.flush_to_file(filename);
    }

    static void write_namespace_scc_owner_ixx(cache const& c, std::string_view const& owner, std::vector<std::string> const& namespaces, std::vector<std::string> const& imports)
    {
        // Emits a module interface unit that "owns" an SCC (strongly-connected component) of namespaces.
        // owner: canonical module name for the SCC (chosen by main.cpp as the lexicographically smallest ns)
        // namespaces: all namespaces in the SCC (including owner)
        // imports: dependencies that are outside of this SCC (imports within the SCC are handled by consolidation)
        // Output $(out)\winrt\<owner>.ixx  (export module owner;)
        // Export module owner, import std, export import winrt.base, and import external deps.
        // Forward-declare all projected types for all namespaces in this SCC BEFORE including any impl headers.
        // This is required because:
        // SCC members frequently have cyclic type references, and
        // generated headers suppress dependent includes when WINRT_MODULE is defined.
        // Include impl headers for all SCC namespaces in phase order: all *.0.h, then all *.1.h, then all *.2.h,
        // then all projection headers. This preserves the original header layering while keeping SCC compilation
        // deterministic.
        writer w;
        write_preamble(w);
        write_module_global_fragment(w);

        w.write("export module %;\n\n", owner);

        w.write("// Module dependencies:\n");
        w.write("//   - std\n");
        w.write("//   - winrt.base (re-exported)\n");

        if (imports.empty())
        {
            w.write("//   - (no additional namespace imports)\n");
        }
        else
        {
            for (auto&& module : imports)
            {
                w.write("//   - %\n", module);
            }
        }

        w.write(R"(
// This module is an SCC owner (cycle breaker); other SCC namespaces are emitted as re-export stubs.
)");

        w.write(R"(
import std;
export import winrt.base;
)");

        for (auto&& module : imports)
        {
            w.write("import %;\n", module);
        }

        w.write('\n');

        w.write(R"(#pragma push_macro("WINRT_EXPORT")
#undef WINRT_EXPORT
#define WINRT_EXPORT export

)");

        // Export forward declarations for all projected types in this SCC. This provides names early enough for any
        // SCC-internal cycles that show up in the impl headers.
        for (auto&& ns : namespaces)
        {
            auto found = c.namespaces().find(ns);

            if (found == c.namespaces().end() || !has_projected_types(found->second))
            {
                continue;
            }

            auto const& members = found->second;
            auto wrap_type = wrap_type_namespace(w, ns);
            w.write_each<write_forward>(members.enums);
            w.write_each<write_forward>(members.interfaces);
            w.write_each<write_forward>(members.classes);
            w.write_each<write_forward>(members.structs);
            w.write_each<write_forward>(members.delegates);
            w.write_each<write_forward>(members.contracts);
        }

        w.write(R"(#pragma pop_macro("WINRT_EXPORT")

)");

        // Pull in the per-namespace impl headers in a stable phase order so that all forward declarations are present
        // before definitions, regardless of SCC member ordering.
        for (auto&& ns : namespaces)
        {
            w.write("#include \"winrt/impl/%.0.h\"\n", ns);
        }

        for (auto&& ns : namespaces)
        {
            w.write("#include \"winrt/impl/%.1.h\"\n", ns);
        }

        for (auto&& ns : namespaces)
        {
            w.write("#include \"winrt/impl/%.2.h\"\n", ns);
        }

        for (auto&& ns : namespaces)
        {
            w.write("#include \"winrt/%.h\"\n", ns);
        }

        auto filename = settings.output_folder + "winrt/";
        filename += owner;
        filename += ".ixx";
        w.flush_to_file(filename);
    }

    static void write_namespace_h(cache const& c, std::string_view const& ns, cache::namespace_members const& members, std::vector<std::string>& module_imports)
    {
        writer w;
        w.type_namespace = ns;

        {
            auto wrap_impl = wrap_impl_namespace(w);
            w.write_each<write_consume_definitions>(members.interfaces);
            w.param_names = true;
            w.write_each<write_delegate_implementation>(members.delegates);
            w.write_each<write_produce>(members.interfaces, c);
            w.write_each<write_dispatch_overridable>(members.classes);
        }
        {
            auto wrap_type = wrap_type_namespace(w, ns);
            w.write_each<write_enum_operators>(members.enums);
            w.write_each<write_class_definitions>(members.classes);
            w.write_each<write_fast_class_base_definitions>(members.classes);
            w.write_each<write_delegate_definition>(members.delegates);
            w.write_each<write_interface_override_methods>(members.classes);
            w.write_each<write_class_override>(members.classes);
        }
        {
            auto wrap_std = wrap_std_namespace(w);

            {
                auto wrap_lean = wrap_lean_and_mean(w);
                w.write_each<write_std_hash>(members.interfaces);
                w.write_each<write_std_hash>(members.classes);
            }
            {
                w.write_each<write_std_formatter>(members.interfaces);
                w.write_each<write_std_formatter>(members.classes);   
            }
        }

        write_namespace_special(w, ns);

        if (settings.modules)
        {
            get_namespace_module_imports(c, ns, w, module_imports);
            write_module_aware_export_macro_pop(w);
        }
        else
        {
            module_imports.clear();
        }
        write_close_file_guard(w);
        w.swap();
        write_preamble(w);
        write_open_file_guard(w, ns);

        if (settings.modules)
        {
            write_module_aware_export_macro_push(w);
            write_module_aware_export_includes_start(w);
        }

        write_version_assert(w);
        write_parent_depends(w, c, ns);

        for (auto&& depends : w.depends)
        {
            w.write_depends(depends.first, '2');
        }

        w.write_depends(w.type_namespace, '2');

        if (settings.modules)
        {
            write_module_aware_export_includes_end(w);
        }

        w.save_header();
    }

    static void write_module_g_cpp(std::vector<TypeDef> const& classes)
    {
        writer w;
        write_preamble(w);
        write_pch(w);
        write_module_g_cpp(w, classes);
        w.flush_to_file(settings.output_folder + "module.g.cpp");
    }

    static void write_component_g_h(TypeDef const& type)
    {
        writer w;
        w.add_depends(type);
        write_component_g_h(w, type);

        w.swap();
        write_preamble(w);
        write_include_guard(w);

        for (auto&& depends : w.depends)
        {
            w.write_depends(depends.first);
        }

        auto filename = settings.output_folder + get_generated_component_filename(type) + ".g.h";
        path folder = filename;
        folder.remove_filename();
        create_directories(folder);
        w.flush_to_file(filename);
    }

    static void write_component_g_cpp(TypeDef const& type)
    {
        if (!settings.component_opt)
        {
            return;
        }

        writer w;
        write_preamble(w);
        write_component_g_cpp(w, type);

        auto filename = settings.output_folder + get_generated_component_filename(type) + ".g.cpp";
        path folder = filename;
        folder.remove_filename();
        create_directories(folder);
        w.flush_to_file(filename);
    }

    static void write_component_h(TypeDef const& type)
    {
        if (settings.component_folder.empty())
        {
            return;
        }

        auto path = settings.component_folder + get_component_filename(type) + ".h";

        if (!settings.component_overwrite && exists(path))
        {
            return;
        }

        writer w;
        write_include_guard(w);
        write_component_h(w, type);
        w.flush_to_file(path);
    }

    static void write_component_cpp(TypeDef const& type)
    {
        if (settings.component_folder.empty())
        {
            return;
        }

        auto path = settings.component_folder + get_component_filename(type) + ".cpp";

        if (!settings.component_overwrite && exists(path))
        {
            return;
        }

        writer w;
        write_pch(w);
        write_component_cpp(w, type);
        w.flush_to_file(path);
    }
}
