#include "pch.h"
#include "cppwinrt_visualizer.h"
#include "object_visualizer.h"
#include "property_visualizer.h"

using namespace Microsoft::VisualStudio::Debugger;
using namespace Microsoft::VisualStudio::Debugger::Evaluation;
using namespace Microsoft::VisualStudio::Debugger::Telemetry;
using namespace Microsoft::VisualStudio::Debugger::DefaultPort;
using namespace std::filesystem;
using namespace winrt;
using namespace winmd::reader;

namespace
{
    std::unique_ptr<cache> db_cache;
    coded_index<TypeDefOrRef> guid_TypeRef{};
    std::set<std::string> loaded_ns;
    std::list<std::string> candidates_files;
    bool winmd_candidates_collected{};
}

coded_index<TypeDefOrRef> FindGuidType()
{
    if (!guid_TypeRef)
    {
        // There is no definitive TypeDef for System.Guid. But there are a variety of TypeRefs scattered about
        // This one should be relatively quick to find
        auto pv = db_cache->find("Windows.Foundation", "IPropertyValue");
        for (auto&& method : pv.MethodList())
        {
            if (method.Name() == "GetGuid")
            {
                auto const& sig = method.Signature();
                auto const& type = sig.ReturnType().Type().Type();
                XLANG_ASSERT(std::holds_alternative<coded_index<TypeDefOrRef>>(type));
                if (std::holds_alternative<coded_index<TypeDefOrRef>>(type))
                {
                    guid_TypeRef = std::get<coded_index<TypeDefOrRef>>(type);
                    XLANG_ASSERT(guid_TypeRef.type() == TypeDefOrRef::TypeRef);
                    XLANG_ASSERT(guid_TypeRef.TypeRef().TypeNamespace() == "System");
                    XLANG_ASSERT(guid_TypeRef.TypeRef().TypeName() == "Guid");
                }
            }
        }
    }
    return guid_TypeRef;
}

void MetadataDiagnostic(DkmProcess* process, std::wstring const& status, std::filesystem::path const& path)
{
    auto message = status + path.native();
    NatvisDiagnostic(process, message, NatvisDiagnosticLevel::Verbose);
}

// Downloads a metadata file from a remote target.
HRESULT DownloadMetadata(DkmProcess* process, std::filesystem::path const& remote_path, std::filesystem::path const& local_path)
{
    auto conn = process->Connection();
    if ((conn->Flags() & DkmTransportConnectionFlags_t::LocalComputer) != 0)
    {
        return E_FAIL;
    }

    com_ptr<DkmString> root_dir;
    IF_FAIL_RET(DkmString::Create(remote_path.parent_path().c_str(), root_dir.put()));
    com_ptr<DkmString> search_spec;
    IF_FAIL_RET(DkmString::Create(remote_path.filename().c_str(), search_spec.put()));
    DkmArray<DkmFileInfo*> results;
    IF_FAIL_RET(conn->GetFileListing(root_dir.get(), search_spec.get(), false, &results));
    if (results.Length != 1)
    {
        return E_FAIL;
    }

    auto& remote_listing = results.Members[0];
    auto remote_file_size = remote_listing->FileSize();
    file_time_type remote_file_time{ file_time_type::duration(remote_listing->LastWriteTime()) };
    auto remote_file_path = remote_listing->FilePath();
    auto remote_file_name = remote_listing->FileName(); remote_file_name;
    if (exists(local_path))
    {
        auto local_file_time = last_write_time(local_path);
        auto local_file_size = file_size(local_path);
        if ((local_file_time >= remote_file_time) && (local_file_size == remote_file_size))
        {
            return S_OK;
        }
    }

    MetadataDiagnostic(process, L"Downloading ", remote_path);
    com_ptr<DkmString> local_file_path;
    IF_FAIL_RET(DkmString::Create(local_path.c_str(), local_file_path.put()));
    IF_FAIL_RET(conn->DownloadFile(remote_file_path, local_file_path.get(), true));
    last_write_time(local_path, remote_file_time);
    return S_OK;
}

// If local file found, use it
// If newer remote file found, download it to cache
// If cached file found (downloaded or not), use it
bool FindMetadata(DkmProcess* process, std::filesystem::path& winmd_path, bool remote)
{
    if (exists(winmd_path))
    {
        return true;
    }

    if (!remote)
    {
        return false;
    }

    auto cached_path = std::filesystem::temp_directory_path();
    cached_path.replace_filename(winmd_path.filename().c_str());
    DownloadMetadata(process, winmd_path, cached_path);
    if (exists(cached_path))
    {
        winmd_path = cached_path;
        return true;
    }

    return false;
}

void AddWinmdCandidate(std::filesystem::path const& candidate)
{
    // path.string() may corrupt non-ASCII paths.
    std::string path_string = winrt::to_string(candidate.native());
    for (auto& c : path_string)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = c - 'A' + 'a';
        }
    }
    if (std::find(candidates_files.begin(), candidates_files.end(), path_string) == candidates_files.end())
    {
        candidates_files.push_back(path_string);
    }
}

void CollectWinmdFromDirectoryLocal(std::filesystem::path const& directory)
{
    try
    {
        for (auto const& entry : std::filesystem::directory_iterator(directory))
        {
            if (std::filesystem::is_regular_file(entry) && entry.path().extension() == L".winmd")
            {
                AddWinmdCandidate(entry.path());
            }
        }
    }
    catch (...)
    {
        // If unable to read metadata, don't take down VS 
    }
}

void CollectWinmdDirectory(DkmProcess* process, std::filesystem::path const& directory, bool remote)
{
    CollectWinmdFromDirectoryLocal(directory);

    if (!remote)
    {
        return;
    }

    auto conn = process->Connection();
    com_ptr<DkmString> remote_dir;
    com_ptr<DkmString> search_spec;
    if (FAILED(DkmString::Create(directory.c_str(), remote_dir.put())) ||
        FAILED(DkmString::Create(L"*.winmd", search_spec.put())))
    {
        return;
    }

    DkmArray<DkmFileInfo*> results;
    if (FAILED(conn->GetFileListing(remote_dir.get(), search_spec.get(), false, &results)))
    {
        return;
    }

    for (UINT32 i = 0; i < results.Length; ++i)
    {
        auto file_path = results.Members[i]->FilePath();
        if (file_path)
        {
            AddWinmdCandidate(file_path->Value());
        }
    }
}

void CollectSystemMetadata()
{
    std::array<wchar_t, MAX_PATH> local{};
#ifdef _WIN64
    ExpandEnvironmentStringsW(L"%windir%\\System32\\WinMetadata", local.data(), static_cast<DWORD>(local.size()));
#else
    ExpandEnvironmentStringsW(L"%windir%\\SysNative\\WinMetadata", local.data(), static_cast<DWORD>(local.size()));
#endif
    CollectWinmdFromDirectoryLocal(local.data());
}

bool EvaluateUInt64(DkmVisualizedExpression* pExpression, wchar_t const* expression, UINT64& value)
{
    com_ptr<DkmString> pEvalText;
    if (FAILED(DkmString::Create(DkmSourceString(expression), pEvalText.put())))
    {
        return false;
    }

    auto evalFlags = DkmEvaluationFlags::TreatAsExpression
                   | DkmEvaluationFlags::ForceEvaluationNow
                   | DkmEvaluationFlags::ForceRealFuncEval;

    auto inspectionContext = pExpression->InspectionContext();

    com_ptr<DkmLanguageExpression> pLanguageExpression;
    if (FAILED(DkmLanguageExpression::Create(inspectionContext->Language(),
        evalFlags, pEvalText.get(), DkmDataItem::Null(), pLanguageExpression.put())))
    {
        return false;
    }

    com_ptr<DkmInspectionContext> pInspectionContext;
    if ((inspectionContext->EvaluationFlags() & evalFlags) != evalFlags)
    {
        if (FAILED(DkmInspectionContext::Create(
            inspectionContext->InspectionSession(),
            inspectionContext->RuntimeInstance(),
            inspectionContext->Thread(),
            inspectionContext->Timeout(),
            evalFlags,
            inspectionContext->FuncEvalFlags(),
            inspectionContext->Radix(),
            inspectionContext->Language(),
            inspectionContext->ReturnValue(),
            pInspectionContext.put())))
        {
            return false;
        }
    }
    else
    {
        pInspectionContext.copy_from(inspectionContext);
    }

    com_ptr<DkmEvaluationResult> pEvaluationResult;
    auto hr = pExpression->EvaluateExpressionCallback(pInspectionContext.get(), pLanguageExpression.get(),
        pExpression->StackFrame(), pEvaluationResult.put());

    if (FAILED(hr) || !pEvaluationResult || pEvaluationResult->TagValue() != DkmEvaluationResult::Tag::SuccessResult)
    {
        return false;
    }

    auto pValue = pEvaluationResult.as<DkmSuccessEvaluationResult>()->Value();
    if (!pValue)
    {
        return false;
    }

    wchar_t* text_end = nullptr;
    auto text = pValue->Value();
    auto parsed = std::wcstoull(text, &text_end, 0);
    if (text_end == text)
    {
        return false;
    }

    value = parsed;
    return true;
}

// Asks the debuggee which the metadata of the references it consumes. The files are embedded as a
// semicolon separated wide string literal, and its size is embedded alongside it so that the string
// can be read out of the debuggee's memory in one exact read.
void CollectKnownMetadata(DkmVisualizedExpression* pExpression, DkmProcess* process)
{
    UINT64 address = 0;
    UINT64 size = 0;
    if (!EvaluateUInt64(pExpression, L"(unsigned long long)WINRT_Known_Winmds", address) ||
        !EvaluateUInt64(pExpression, L"(unsigned long long)WINRT_Known_Winmds_Size", size) ||
        !address || !size)
    {
        return;
    }

    CAutoDkmArray<BYTE> stringMemory;
    auto hr = process->ReadMemoryString(address, DkmReadMemoryFlags::None, sizeof(wchar_t),
        static_cast<UINT32>(size / sizeof(wchar_t)), &stringMemory);
    if (FAILED(hr))
    {
        return;
    }

    // The buffer includes the null terminator, which the list have no use for.
    auto const characters = stringMemory.Length / sizeof(wchar_t);
    std::wstring_view dir_list(reinterpret_cast<wchar_t const*>(stringMemory.Members), characters - 1);

    size_t start = 0;
    while (start <= dir_list.size())
    {
        auto end = dir_list.find(L';', start);
        if (end == std::wstring_view::npos)
        {
            end = dir_list.size();
        }

        if (end != start)
        {
            AddWinmdCandidate(std::filesystem::path(dir_list.substr(start, end - start)));
        }

        start = end + 1;
    }
}

void EnsureWinmdCandidatesCollected(DkmVisualizedExpression* pExpression, WCHAR const* processPath)
{
    if (winmd_candidates_collected)
    {
        return;
    }
    winmd_candidates_collected = true;

    auto process = pExpression->RuntimeInstance()->Process();
    bool remote = (process->Connection()->Flags() & DkmTransportConnectionFlags_t::LocalComputer) == 0;
    CollectKnownMetadata(pExpression, process);
    CollectWinmdDirectory(process, std::filesystem::path(processPath).parent_path(), remote);
    CollectSystemMetadata();
}

// The file that defines a type is named after the type's namespace, in lower case, so the
// namespace of a type name doubles as the name of the file to look it up in.
std::string ToLowerCasedWinmdName(std::string_view const& typeName)
{
    std::string result(typeName);
    auto pos = result.rfind('.');
    if (pos == std::string::npos)
    {
        result.clear();
    }
    else
    {
        result.resize(pos);
    }

    for (auto& c : result)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

// If type not indexed, simulate RoGetMetaDataFile's strategy for finding app-local metadata
// and add to the database dynamically.  RoGetMetaDataFile looks for types in the current process
// so cannot be called directly.
void LoadMetadata(DkmVisualizedExpression* pExpression, std::string_view const& typeName)
{
    auto process = pExpression->RuntimeInstance()->Process();
    auto processPath = process->Path()->Value();
    EnsureWinmdCandidatesCollected(pExpression, processPath);

    bool remote = (process->Connection()->Flags() & DkmTransportConnectionFlags_t::LocalComputer) == 0;
    auto ns = ToLowerCasedWinmdName(typeName);

    while (!ns.empty())
    {
        // A namespace that has been looked at before must not be looked at again.
        if (loaded_ns.insert(ns).second)
        {
            auto winmd_name = ns + ".winmd";
            for (auto it = candidates_files.begin(); it != candidates_files.end(); ++it)
            {
                std::filesystem::path path(*it);
                if (path.filename().string() == winmd_name)
                {
                    auto candidate = path;
                    candidates_files.erase(it);
                    if (FindMetadata(process, candidate, remote))
                    {
                        try
                        {
                            db_cache->add_database(candidate.string(), [](TypeDef const& type) {
                                return type.Flags().WindowsRuntime();
                            });
                        }
                        catch (...)
                        {
                            NatvisDiagnostic(pExpression,
                                L"Unable to load metadata " + candidate.native(),
                                NatvisDiagnosticLevel::Warning);
                        }
                    }
                    break;
                }
            }
        }
        auto dot = ns.rfind('.');
        if (dot == std::string::npos)
        {
            break;
        }
        ns.resize(dot);
    }
}

TypeDef FindSimpleType(DkmVisualizedExpression* pExpression, std::string_view const& typeName)
{
    XLANG_ASSERT(typeName.find('<') == std::string_view::npos);
    auto type = db_cache->find(typeName);
    if (type)
    {
        return type;
    }
    // If a namespace has already attempted to load the winmd, then skip it.
    if (loaded_ns.count(ToLowerCasedWinmdName(typeName)) != 0)
    {
        NatvisDiagnostic(pExpression,
            std::wstring(L"Could not find metadata for ") + std::wstring(typeName.begin(), typeName.end()),
            NatvisDiagnosticLevel::Error);
        return {};
    }
    LoadMetadata(pExpression, typeName);
    type = db_cache->find(typeName);
    if (!type)
    {
        NatvisDiagnostic(pExpression,
            std::wstring(L"Could not find metadata for ") + std::wstring(typeName.begin(), typeName.end()),
            NatvisDiagnosticLevel::Error);
    }
    return type;
}

TypeDef FindSimpleType(DkmVisualizedExpression* pExpression, std::string_view const& typeNamespace, std::string_view const& typeName)
{
    XLANG_ASSERT(typeName.find('<') == std::string_view::npos);
    auto type = db_cache->find(typeNamespace, typeName);
    if (!type)
    {
        std::string fullName(typeNamespace);
        fullName.append(".");
        fullName.append(typeName);
        return FindSimpleType(pExpression, fullName);
    }
    return type;
}

std::vector<std::string> ParseTypeName(std::string_view name)
{
    DWORD count;
    HSTRING* parts;
    auto wide_name = winrt::to_hstring(name);
    winrt::check_hresult(::RoParseTypeName(static_cast<HSTRING>(get_abi(wide_name)), &count, &parts));

    winrt::com_array<winrt::hstring> wide_parts{ parts, count, winrt::take_ownership_from_abi };
    std::vector<std::string> result;
    for (auto&& part : wide_parts)
    {
        result.push_back(winrt::to_string(part));
    }
    return result;
}

template <std::input_iterator iter, std::sentinel_for<iter> sent>
TypeSig ResolveGenericTypePart(DkmVisualizedExpression* pExpression, iter& it, sent const& end)
{
    constexpr std::pair<std::string_view, ElementType> elementNames[] = {
        {"Boolean", ElementType::Boolean},
        {"Int8", ElementType::I1},
        {"Int16", ElementType::I2},
        {"Int32", ElementType::I4},
        {"Int64", ElementType::I8},
        {"UInt8", ElementType::U1},
        {"UInt16", ElementType::U2},
        {"UInt32", ElementType::U4},
        {"UInt64", ElementType::U8},
        {"Single", ElementType::R4},
        {"Double", ElementType::R8},
        {"String", ElementType::String},
        {"Char16", ElementType::Char},
        {"Object", ElementType::Object}
    };
    std::string_view partName = *it;
    auto basic_type_pos = std::find_if(std::begin(elementNames), std::end(elementNames), [&partName](auto&& elem) { return elem.first == partName; });
    if (basic_type_pos != std::end(elementNames))
    {
        return TypeSig{ basic_type_pos->second };
    }

    if (partName == "Guid")
    {
        return TypeSig{ FindGuidType() };
    }
    
    TypeDef type = FindSimpleType(pExpression, partName);
    auto tickPos = partName.rfind('`');
    if (tickPos == partName.npos)
    {
        return TypeSig{ type.coded_index<TypeDefOrRef>() };
    }

    int paramCount = 0;
    std::from_chars(partName.data() + tickPos + 1, partName.data() + partName.size(), paramCount);
    std::vector<TypeSig> genericArgs;
    for (int i = 0; i < paramCount; ++i)
    {
        genericArgs.push_back(ResolveGenericTypePart(pExpression, ++it, end));
    }
    return TypeSig{ GenericTypeInstSig{ type.coded_index<TypeDefOrRef>(), std::move(genericArgs) } };
}

TypeSig ResolveGenericType(DkmVisualizedExpression* pExpression, std::string_view genericName)
{
    auto parts = ParseTypeName(genericName);
    auto begin = parts.begin();
    return ResolveGenericTypePart(pExpression, begin, parts.end());
}

TypeSig FindType(DkmVisualizedExpression* pExpression, std::string_view const& typeName)
{
    auto paramIndex = typeName.find('<');
    if (paramIndex == std::string_view::npos)
    {
        auto type = FindSimpleType(pExpression, typeName);
        if (!type)
        {
            return TypeSig{ ElementType::End };
        }
        return TypeSig{ type.coded_index<TypeDefOrRef>() };
    }
    else
    {
        return ResolveGenericType(pExpression, typeName);
    }
}

cppwinrt_visualizer::cppwinrt_visualizer()
{
    db_cache = std::make_unique<cache>();
    // Log an event for telemetry purposes when the visualizer is brought online
    com_ptr<DkmString> eventName;
    if SUCCEEDED(DkmString::Create(DkmSourceString(L"vs/vc/diagnostics/cppwinrtvisualizer/objectconstructed"), eventName.put()))
    {
        com_ptr<DkmTelemetryEvent> error;
        if SUCCEEDED(DkmTelemetryEvent::Create(eventName.get(), nullptr, nullptr, error.put()))
        {
            error->Post();
        }
    }
}

cppwinrt_visualizer::~cppwinrt_visualizer()
{
    ClearTypeResolver();
    guid_TypeRef = {};
    loaded_ns.clear();
    winmd_candidates_collected = {};
    candidates_files.clear();
    db_cache.reset();
}

HRESULT cppwinrt_visualizer::EvaluateVisualizedExpression(
    _In_ DkmVisualizedExpression* pVisualizedExpression,
    _COM_Outptr_result_maybenull_ DkmEvaluationResult** ppResultObject
)
{
    try
    {
        com_ptr<IUnknown> pUnkTypeSymbol;
        IF_FAIL_RET(pVisualizedExpression->GetSymbolInterface(__uuidof(IDiaSymbol), pUnkTypeSymbol.put()));

        com_ptr<IDiaSymbol> pTypeSymbol = pUnkTypeSymbol.as<IDiaSymbol>();

        CComBSTR bstrTypeName;
        IF_FAIL_RET(pTypeSymbol->get_name(&bstrTypeName));

        // Visualize top-level C++/WinRT objects containing ABI pointers
        ObjectType objectType;
        if (wcscmp(bstrTypeName, L"winrt::Windows::Foundation::IInspectable") == 0)
        {
            objectType = ObjectType::Projection;
        }
        // Visualize nested object properties via raw ABI pointers
        else if ((wcscmp(bstrTypeName, L"winrt::impl::IInspectable") == 0) ||
                 (wcscmp(bstrTypeName, L"winrt::impl::inspectable_abi") == 0))
        {
            objectType = ObjectType::Abi;
        }
        // Visualize C++/WinRT object implementations
        else if (wcsncmp(bstrTypeName, L"winrt::impl::producer<", wcslen(L"winrt::impl::producer<")) == 0)
        {
            objectType = ObjectType::Abi;
        }
        // Visualize all raw IInspectable pointers
        else if (wcscmp(bstrTypeName, L"IInspectable") == 0)
        {
            objectType = ObjectType::Abi;
        }
        else
        {
            // unrecognized type
            NatvisDiagnostic(pVisualizedExpression, 
                std::wstring(L"Unrecognized type: ") + (LPWSTR)bstrTypeName,  NatvisDiagnosticLevel::Error);
            *ppResultObject = nullptr;
            return S_OK;
        }

        IF_FAIL_RET(object_visualizer::CreateEvaluationResult(pVisualizedExpression, objectType, ppResultObject));

        return S_OK;
    }
    catch (...)
    {
        // If something goes wrong, just fail to display object/property.  Don't take down VS.
        NatvisDiagnostic(pVisualizedExpression, 
            L"Exception in cppwinrt_visualizer::EvaluateVisualizedExpression", NatvisDiagnosticLevel::Error, to_hresult());
        return E_FAIL;
    }
}

HRESULT cppwinrt_visualizer::UseDefaultEvaluationBehavior(
    _In_ DkmVisualizedExpression* /*pVisualizedExpression*/,
    _Out_ bool* pUseDefaultEvaluationBehavior,
    _Deref_out_opt_ DkmEvaluationResult** ppDefaultEvaluationResult
)
{
    *pUseDefaultEvaluationBehavior = false;
    *ppDefaultEvaluationResult = nullptr;

    return S_OK;
}

HRESULT cppwinrt_visualizer::GetChildren(
    _In_ DkmVisualizedExpression* pVisualizedExpression,
    _In_ UINT32 InitialRequestSize,
    _In_ DkmInspectionContext* pInspectionContext,
    _Out_ DkmArray<DkmChildVisualizedExpression*>* pInitialChildren,
    _Deref_out_ DkmEvaluationResultEnumContext** ppEnumContext
)
{
    try
    {
        com_ptr<object_visualizer> pObjectVisualizer;
        HRESULT hr = pVisualizedExpression->GetDataItem(pObjectVisualizer.put());
        if (SUCCEEDED(hr))
        {
            IF_FAIL_RET(pObjectVisualizer->GetChildren(InitialRequestSize, pInspectionContext, pInitialChildren, ppEnumContext));
        }
        else
        {
            com_ptr<property_visualizer> pPropertyVisualizer;
            hr = pVisualizedExpression->GetDataItem(pPropertyVisualizer.put());
            if (SUCCEEDED(hr))
            {
                IF_FAIL_RET(pPropertyVisualizer->GetChildren(InitialRequestSize, pInspectionContext, pInitialChildren, ppEnumContext));
            }
        }

        return hr;
    }
    catch (...)
    {
        // If something goes wrong, just fail to display object/property.  Don't take down VS.
        NatvisDiagnostic(pVisualizedExpression,
            L"Exception in cppwinrt_visualizer::GetChildren", NatvisDiagnosticLevel::Error, to_hresult());
        return E_FAIL;
    }
}

HRESULT cppwinrt_visualizer::GetItems(
    _In_ DkmVisualizedExpression* pVisualizedExpression,
    _In_ DkmEvaluationResultEnumContext* pEnumContext,
    _In_ UINT32 StartIndex,
    _In_ UINT32 Count,
    _Out_ DkmArray<DkmChildVisualizedExpression*>* pItems
)
{
    try
    {
        com_ptr<object_visualizer> pObjectVisualizer;
        HRESULT hr = pVisualizedExpression->GetDataItem(pObjectVisualizer.put());
        if (SUCCEEDED(hr))
        {
            IF_FAIL_RET(pObjectVisualizer->GetItems(pVisualizedExpression, pEnumContext, StartIndex, Count, pItems));
        }
        else
        {
            com_ptr<property_visualizer> pPropertyVisualizer;
            hr = pVisualizedExpression->GetDataItem(pPropertyVisualizer.put());
            if (SUCCEEDED(hr))
            {
                IF_FAIL_RET(pPropertyVisualizer->GetItems(pEnumContext, StartIndex, Count, pItems));
            }
        }

        return hr;
    }
    catch (...)
    {
        // If something goes wrong, just fail to display object/property.  Don't take down VS.
        NatvisDiagnostic(pVisualizedExpression,
            L"Exception in cppwinrt_visualizer::GetItems", NatvisDiagnosticLevel::Error, to_hresult());
        return E_FAIL;
    }
}

HRESULT cppwinrt_visualizer::SetValueAsString(
    _In_ DkmVisualizedExpression* pVisualizedExpression,
    _In_ DkmString* pValue,
    _In_ UINT32 Timeout,
    _Deref_out_opt_ DkmString** ppErrorText
)
{
    try
    {
        com_ptr<property_visualizer> pPropertyVisualizer;
        HRESULT hr = pVisualizedExpression->GetDataItem(pPropertyVisualizer.put());
        if (SUCCEEDED(hr))
        {
            IF_FAIL_RET(pPropertyVisualizer->SetValueAsString(pValue, Timeout, ppErrorText));
        }

        return hr;
    }
    catch (...)
    {
        // If something goes wrong, just fail to update object/property.  Don't take down VS.
        NatvisDiagnostic(pVisualizedExpression, 
            L"Exception in cppwinrt_visualizer::SetValueAsString", NatvisDiagnosticLevel::Error, to_hresult());
        return E_FAIL;
    }
}

HRESULT cppwinrt_visualizer::GetUnderlyingString(
    _In_ DkmVisualizedExpression* pVisualizedExpression,
    _Deref_out_opt_ DkmString** ppStringValue
)
{
    try
    {
        com_ptr<property_visualizer> pPropertyVisualizer;
        HRESULT hr = pVisualizedExpression->GetDataItem(pPropertyVisualizer.put());
        if (SUCCEEDED(hr))
        {
            IF_FAIL_RET(pPropertyVisualizer->GetUnderlyingString(ppStringValue));
        }

        return hr;
    }
    catch (...)
    {
        // If something goes wrong, just fail to display object/property.  Don't take down VS.
        NatvisDiagnostic(pVisualizedExpression->RuntimeInstance()->Process(),
            L"Exception in cppwinrt_visualizer::GetUnderlyingString", NatvisDiagnosticLevel::Error, to_hresult());
        return E_FAIL;
    }
}
