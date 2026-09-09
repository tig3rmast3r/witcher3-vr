#include "managed_runtime.h"

#include <Windows.h>

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace w3vr {
namespace {

namespace fs = std::filesystem;

// [TRIAL:RENDERER-FIRST-INTEGRATION-SWITCH V23024 1/1] Active root aliases
// are copied from dedicated reference directories. No launcher path writes,
// moves, renames or deletes anything inside a reference directory.
constexpr wchar_t kModReference[] = L"witcher3vr-mod-reference";
constexpr wchar_t kReshadeReference[] = L"witcher3vr-reshade-reference";
constexpr wchar_t kReshadeDlss5Reference[] =
    L"witcher3vr-reshade-dlss5-reference";
constexpr wchar_t kOptiscalerReference[] = L"witcher3vr-optiscaler-reference";
constexpr wchar_t kOptiscalerDlss5Reference[] =
    L"witcher3vr-optiscaler-dlss5-reference";
constexpr wchar_t kDlss5Reference[] = L"witcher3vr-dlss5-reference";
constexpr wchar_t kReshadeRuntime[] = L"ReShade64.dll";
constexpr wchar_t kDlss5Addon[] = L"renodx-dlss5-v2.5.addon64";
constexpr char kDlss5AddonToken[] = "renodx-dlss5-v2.5.addon64";

constexpr auto kDlss5Files = std::to_array<const wchar_t*>({
    L"nvngx_dlss.dll", L"nvngx_dlssg.dll", L"nvngx_dlssnr.dll"});
// [FIX:LEAN-DLSS5-REFERENCE V23031 1/1] Publish only the files used by the
// selected Witcher 3 routes. The FidelityFX DLLs already shipped by the game
// remain untouched, while optional FSR/XeSS/FG backends are not packaged.
constexpr auto kCanonicalOptiscalerFiles = std::to_array<const wchar_t*>({
    L"OptiScaler.dll", L"OptiScaler.ini"});
constexpr auto kDlss5OptiscalerFiles = std::to_array<const wchar_t*>({
    L"OptiScaler.dll", L"OptiScaler.ini", L"nvngx.dll_dlssnr.dll"});
constexpr auto kLegacyOptiscalerOptionalFiles =
    std::to_array<const wchar_t*>({
        L"OptiScaler/amd_fidelityfx_framegeneration_dx12.dll",
        L"OptiScaler/amd_fidelityfx_loader_dx12.dll",
        L"OptiScaler/amd_fidelityfx_upscaler_dx12.dll",
        L"OptiScaler/amd_fidelityfx_vk.dll",
        L"OptiScaler/libxell.dll",
        L"OptiScaler/libxess.dll",
        L"OptiScaler/libxess_dx11.dll",
        L"OptiScaler/libxess_fg.dll",
        L"OptiScaler/D3D12_OptiScaler/D3D12Core.dll"});

struct CopyOperation {
    fs::path source;
    fs::path destination;
    fs::path staged;
    bool preserve_existing{};
};

std::wstring WindowsError(DWORD code) {
    wchar_t* buffer{};
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = length != 0 && buffer != nullptr
        ? std::wstring(buffer, length)
        : L"Windows error " + std::to_wstring(code);
    if (buffer != nullptr) LocalFree(buffer);
    while (!result.empty() &&
        (result.back() == L'\r' || result.back() == L'\n')) {
        result.pop_back();
    }
    return result;
}

bool IsNotFound(DWORD code) {
    return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND;
}

bool InspectRegularFile(
    const fs::path& path, bool& present, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (IsNotFound(code)) {
            present = false;
            return true;
        }
        error = L"Could not inspect integration file:\n" + path.wstring() +
            L"\n\n" + WindowsError(code);
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"Expected a direct regular integration file:\n" +
            path.wstring();
        return false;
    }
    present = true;
    return true;
}

bool RequireReferenceFile(const fs::path& path, std::wstring& error) {
    bool present{};
    if (!InspectRegularFile(path, present, error)) return false;
    if (present) return true;
    error = L"Required integration reference file is missing:\n" +
        path.wstring();
    return false;
}

bool EnsureDirectory(const fs::path& path, std::wstring& error) {
    std::error_code code;
    fs::create_directories(path, code);
    if (code) {
        const std::string narrow = code.message();
        error = L"Could not create integration directory:\n" +
            path.wstring() + L"\n\n" +
            std::wstring(narrow.begin(), narrow.end());
        return false;
    }
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"Integration directory is not a direct directory:\n" +
            path.wstring();
        return false;
    }
    return true;
}

bool MakeWritableIfPresent(const fs::path& path, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (IsNotFound(code)) return true;
        error = L"Could not inspect active integration alias:\n" +
            path.wstring() + L"\n\n" + WindowsError(code);
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"Refusing to replace a non-regular integration alias:\n" +
            path.wstring();
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) return true;
    if (SetFileAttributesW(path.c_str(),
            attributes & ~FILE_ATTRIBUTE_READONLY)) {
        return true;
    }
    const DWORD code = GetLastError();
    // [FIX:IDEMPOTENT-RUNTIME-CLEANUP V23025 1/2] Every optional alias and
    // staging cleanup accepts a file disappearing after inspection. This can
    // happen when a prior launcher instance or a security scanner finishes
    // cleaning the same temporary file.
    if (IsNotFound(code)) return true;
    error = L"Could not make active integration alias writable:\n" +
        path.wstring() + L"\n\n" + WindowsError(code);
    return false;
}

bool RemoveFileIfPresent(const fs::path& path, std::wstring& error) {
    bool present{};
    if (!InspectRegularFile(path, present, error)) return false;
    if (!present) return true;
    if (!MakeWritableIfPresent(path, error)) return false;
    if (DeleteFileW(path.c_str())) return true;
    const DWORD code = GetLastError();
    // [FIX:IDEMPOTENT-RUNTIME-CLEANUP V23025 2/2] Apply the same not-found
    // success rule to every inactive root alias and every staged runtime or
    // ReShade INI file that is removed through this shared helper.
    if (IsNotFound(code)) return true;
    error = L"Could not remove inactive integration alias:\n" +
        path.wstring() + L"\n\n" + WindowsError(code);
    return false;
}

bool RemoveDirectoryIfEmpty(const fs::path& path, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (IsNotFound(code)) return true;
        error = L"Could not inspect legacy integration directory:\n" +
            path.wstring() + L"\n\n" + WindowsError(code);
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"Refusing to remove a non-directory legacy integration "
            L"path:\n" + path.wstring();
        return false;
    }
    if (RemoveDirectoryW(path.c_str())) return true;
    const DWORD code = GetLastError();
    // Unknown user files are never recursively deleted. A non-empty legacy
    // directory is harmless after every known managed payload is removed.
    if (IsNotFound(code) || code == ERROR_DIR_NOT_EMPTY) return true;
    error = L"Could not remove empty legacy integration directory:\n" +
        path.wstring() + L"\n\n" + WindowsError(code);
    return false;
}

bool CleanupLegacyOptiscalerPayload(
    const fs::path& root, std::wstring& error) {
    // [FIX:EXACT-INTEGRATION-COMPOSITION V23032 1/1] V23031 stopped
    // distributing the optional backend tree but did not retire copies left
    // in the game root by earlier launcher modes. Remove only the exact known
    // managed files, then prune their directories when empty.
    for (const auto* relative : kLegacyOptiscalerOptionalFiles) {
        if (!RemoveFileIfPresent(root / relative, error)) return false;
    }
    return RemoveDirectoryIfEmpty(
               root / L"OptiScaler/D3D12_OptiScaler", error) &&
        RemoveDirectoryIfEmpty(root / L"OptiScaler", error);
}

void CleanupStaged(std::vector<CopyOperation>& operations) {
    for (auto& operation : operations) {
        if (operation.staged.empty()) continue;
        std::wstring ignored;
        RemoveFileIfPresent(operation.staged, ignored);
    }
}

bool StageCopy(CopyOperation& operation, std::wstring& error) {
    if (operation.preserve_existing) {
        bool present{};
        if (!InspectRegularFile(operation.destination, present, error)) {
            return false;
        }
        if (present) return true;
    }
    if (!RequireReferenceFile(operation.source, error) ||
        !EnsureDirectory(operation.destination.parent_path(), error)) {
        return false;
    }
    operation.staged = operation.destination.wstring() +
        L".w3vr-v23032-next";
    if (!RemoveFileIfPresent(operation.staged, error)) return false;
    if (!CopyFileW(operation.source.c_str(), operation.staged.c_str(), FALSE)) {
        error = L"Could not stage integration reference:\n" +
            operation.source.wstring() + L"\n\nto:\n" +
            operation.destination.wstring() + L"\n\n" +
            WindowsError(GetLastError());
        return false;
    }
    return MakeWritableIfPresent(operation.staged, error);
}

bool PublishCopy(CopyOperation& operation, std::wstring& error) {
    if (operation.staged.empty()) return true;
    if (!MakeWritableIfPresent(operation.destination, error)) return false;
    if (MoveFileExW(operation.staged.c_str(), operation.destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        operation.staged.clear();
        return true;
    }
    error = L"Could not publish integration alias:\n" +
        operation.destination.wstring() + L"\n\n" +
        WindowsError(GetLastError());
    return false;
}

bool SameFilename(std::wstring_view left, std::wstring_view right) {
    return left.size() == right.size() &&
        _wcsnicmp(left.data(), right.data(), left.size()) == 0;
}

bool ValidateDlss5Reference(const fs::path& directory, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"The DLSS5 reference directory is missing or invalid:\n" +
            directory.wstring();
        return false;
    }
    std::array<bool, kDlss5Files.size()> found{};
    WIN32_FIND_DATAW entry{};
    const auto pattern = directory / L"*";
    HANDLE search = FindFirstFileW(pattern.c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) {
        error = L"Could not enumerate the DLSS5 reference directory:\n" +
            directory.wstring() + L"\n\n" + WindowsError(GetLastError());
        return false;
    }
    do {
        const std::wstring_view name(entry.cFileName);
        if (name == L"." || name == L"..") continue;
        size_t index{};
        for (; index < kDlss5Files.size(); ++index) {
            if (SameFilename(name, kDlss5Files[index])) break;
        }
        if (index == kDlss5Files.size() ||
            (entry.dwFileAttributes &
                (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) !=
                0) {
            FindClose(search);
            error = L"The DLSS5 reference must contain only the three NVIDIA "
                L"DLLs. Unexpected entry:\n" +
                (directory / entry.cFileName).wstring();
            return false;
        }
        found[index] = true;
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    for (size_t index = 0; index < found.size(); ++index) {
        if (!found[index]) {
            error = L"The DLSS5 reference is incomplete. Missing:\n" +
                (directory / kDlss5Files[index]).wstring();
            return false;
        }
    }
    return true;
}

std::string Trim(std::string_view value) {
    size_t begin{};
    while (begin < value.size() &&
        std::isspace(static_cast<unsigned char>(value[begin])) != 0) ++begin;
    size_t end = value.size();
    while (end > begin &&
        std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) --end;
    return std::string(value.substr(begin, end - begin));
}

bool SameToken(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
        _strnicmp(left.data(), right.data(), left.size()) == 0;
}

void SetCsvToken(IniDocument& document, const std::string& section,
    const std::string& key, std::string_view token, bool enabled) {
    std::vector<std::string> tokens;
    const std::string current = document.Get(section, key).value_or("");
    size_t begin{};
    while (begin <= current.size()) {
        const size_t separator = current.find(',', begin);
        const size_t end = separator == std::string::npos
            ? current.size() : separator;
        std::string candidate = Trim(
            std::string_view(current).substr(begin, end - begin));
        if (!candidate.empty() && !SameToken(candidate, token)) {
            tokens.push_back(std::move(candidate));
        }
        if (separator == std::string::npos) break;
        begin = separator + 1;
    }
    if (enabled) tokens.emplace_back(token);
    if (tokens.empty()) {
        document.Remove(section, key);
        return;
    }
    std::string value;
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index != 0) value += ',';
        value += tokens[index];
    }
    document.Set(section, key, value);
}

bool WriteIni(const fs::path& path, const std::string& contents,
    std::wstring& error) {
    const fs::path temporary = path.wstring() + L".w3vr-v23032-next";
    if (!RemoveFileIfPresent(temporary, error) ||
        !EnsureDirectory(path.parent_path(), error)) return false;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
            static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output.good()) {
            error = L"Could not stage persistent ReShade configuration:\n" +
                temporary.wstring();
            return false;
        }
    }
    if (!MakeWritableIfPresent(path, error)) return false;
    if (MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    error = L"Could not publish persistent ReShade configuration:\n" +
        path.wstring() + L"\n\n" + WindowsError(GetLastError());
    std::wstring ignored;
    RemoveFileIfPresent(temporary, ignored);
    return false;
}

bool ConfigurePersistentReshade(const fs::path& root, bool use_reshade,
    bool use_dlss5_addon, std::wstring& error) {
    const auto path = root / L"ReShade.ini";
    bool present{};
    if (!InspectRegularFile(path, present, error)) return false;
    if (!present && !use_reshade) return true;
    IniDocument document = IniDocument::FromText("");
    if (present) {
        const auto loaded = IniDocument::Load(path, error);
        if (!loaded) return false;
        document = *loaded;
    }
    document.Set("PROXY", "EnableProxyLibrary", "0");
    document.Set("PROXY", "ProxyLibrary", "");
    document.Set("INPUT", "KeyOverlay", "115,0,0,0");
    SetCsvToken(document, "ADDON", "DisabledAddons", "Generic Depth", true);
    SetCsvToken(document, "ADDON", "LoadFromDllMain", kDlss5AddonToken,
        use_dlss5_addon);
    // Retire the removed add-on from an existing Alpha 1 configuration.
    SetCsvToken(document, "ADDON", "LoadFromDllMain",
        "CheekyFoveatedDLSS.addon64", false);
    if (use_dlss5_addon) {
        if (const auto hooks = document.Get("RenoDX.DLSS5", "EnableHooks");
            hooks && Trim(*hooks) != "2") {
            error = L"ReShade DLSS5 requires [RenoDX.DLSS5] EnableHooks=2 "
                L"for Witcher 3's Streamline 1.5 runtime.";
            return false;
        }
        document.Set("RenoDX.DLSS5", "EnableHooks", "2");
    }
    return WriteIni(path, document.Serialize(), error);
}

template <size_t Count>
void AppendCopies(std::vector<CopyOperation>& operations,
    const fs::path& reference, const fs::path& root,
    const std::array<const wchar_t*, Count>& files) {
    for (const auto* relative : files) {
        operations.push_back({
            reference / relative,
            root / relative,
            {},
            SameFilename(relative, L"OptiScaler.ini")});
    }
}

} // namespace

bool ApplyManagedIntegrationMode(const std::filesystem::path& root,
    IntegrationMode desired, std::wstring& error) {
    if (static_cast<size_t>(desired) >=
        static_cast<size_t>(IntegrationMode::Count)) {
        error = L"The selected integration mode is invalid.";
        return false;
    }
    const bool use_reshade = IntegrationModeUsesReshade(desired);
    const bool use_dlss5 = IntegrationModeUsesDlss5(desired);
    const bool use_modified_optiscaler =
        desired == IntegrationMode::OptiscalerDlss5;
    const bool use_dlss5_addon = desired == IntegrationMode::ReshadeDlss5;
    const auto mod_reference = root / kModReference;
    const auto reshade_reference = root / kReshadeReference;
    const auto reshade_dlss5_reference = root / kReshadeDlss5Reference;
    const auto optiscaler_reference = root /
        (use_modified_optiscaler
            ? kOptiscalerDlss5Reference : kOptiscalerReference);
    const auto dlss5_reference = root / kDlss5Reference;
    if (use_dlss5 && !ValidateDlss5Reference(dlss5_reference, error)) {
        return false;
    }

    std::vector<CopyOperation> operations;
    if (use_reshade) {
        operations.push_back({reshade_reference / kReshadeRuntime,
            root / kReshadeRuntime, {}});
    }
    if (use_modified_optiscaler) {
        AppendCopies(operations, optiscaler_reference, root,
            kDlss5OptiscalerFiles);
    } else {
        AppendCopies(operations, optiscaler_reference, root,
            kCanonicalOptiscalerFiles);
    }
    if (use_dlss5) {
        AppendCopies(operations, dlss5_reference, root, kDlss5Files);
    }
    if (use_dlss5_addon) {
        operations.push_back({reshade_dlss5_reference / kDlss5Addon,
            root / kDlss5Addon, {}});
    }
    // The known-good renderer remains the DXGI proxy in every mode. It loads
    // the adjacent ReShade64.dll secondarily only for ReShade selections.
    operations.push_back({mod_reference / L"dxgi.dll",
        root / L"dxgi.dll", {}});

    for (auto& operation : operations) {
        if (!StageCopy(operation, error)) {
            CleanupStaged(operations);
            return false;
        }
    }
    if (!ConfigurePersistentReshade(
            root, use_reshade, use_dlss5_addon, error) ||
        !CleanupLegacyOptiscalerPayload(root, error) ||
        (!use_modified_optiscaler &&
            !RemoveFileIfPresent(root / L"nvngx.dll_dlssnr.dll", error)) ||
        (!use_reshade &&
            !RemoveFileIfPresent(root / kReshadeRuntime, error)) ||
        !RemoveFileIfPresent(root / L"witcher3vr_dxgi.dll", error) ||
        (!use_dlss5_addon &&
            !RemoveFileIfPresent(root / kDlss5Addon, error)) ||
        !RemoveFileIfPresent(
            root / L"CheekyFoveatedDLSS.addon64", error)) {
        CleanupStaged(operations);
        return false;
    }
    for (auto& operation : operations) {
        if (!PublishCopy(operation, error)) {
            CleanupStaged(operations);
            return false;
        }
    }
    return true;
}

} // namespace w3vr
