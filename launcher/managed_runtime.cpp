#include "managed_runtime.h"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace w3vr {
namespace {

namespace fs = std::filesystem;

// [TRIAL:RENDERER-FIRST-INTEGRATION-SWITCH V23024 1/1] Active root aliases
// are copied from dedicated reference directories. No launcher path writes,
// moves, renames or deletes anything inside a reference directory.
constexpr wchar_t kModReference[] = L"witcher3vr-mod-reference";
constexpr wchar_t kReshadeReference[] = L"witcher3vr-reshade-reference";
constexpr wchar_t kOptiscalerDlss5Reference[] =
    L"witcher3vr-optiscaler-dlss5-reference";
constexpr wchar_t kDlss5Reference[] = L"witcher3vr-dlss5-reference";
constexpr wchar_t kReshadeRuntime[] = L"ReShade64.dll";

constexpr auto kDlss5Files = std::to_array<const wchar_t*>({
    L"nvngx_dlssnr.dll"});
// [FIX:LEAN-DLSS5-REFERENCE V23031 1/1] Publish only the files used by the
// selected Witcher 3 routes. The FidelityFX DLLs already shipped by the game
// remain untouched, while optional FSR/XeSS/FG backends are not packaged.
constexpr auto kDlss5OptiscalerFiles = std::to_array<const wchar_t*>({
    L"OptiScaler.dll", L"nvngx.dll_dlssnr.dll",
    L"amd_fidelityfx_framegeneration_dx12.dll",
    L"ofxr_amd_fidelityfx_framegeneration_dx12.dll"});
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
    // success rule to every inactive root alias and staged runtime file.
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

bool ValidateDlss5Reference(const fs::path& directory, std::wstring& error) {
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        error = L"The DLSS5 reference directory is missing or invalid:\n" +
            directory.wstring();
        return false;
    }
    return RequireReferenceFile(directory / kDlss5Files.front(), error);
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
            fs::path(relative).filename() == L"OptiScaler.ini"});
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
    const bool use_optiscaler = IntegrationModeUsesOptiscaler(desired);
    const auto mod_reference = root / kModReference;
    const auto reshade_reference = root / kReshadeReference;
    const auto optiscaler_reference = root / kOptiscalerDlss5Reference;
    const auto dlss5_reference = root / kDlss5Reference;
    if (use_optiscaler && !ValidateDlss5Reference(dlss5_reference, error)) {
        return false;
    }

    std::vector<CopyOperation> operations;
    if (use_reshade) {
        operations.push_back({reshade_reference / kReshadeRuntime,
            root / kReshadeRuntime, {}});
    }
    if (use_optiscaler) {
        AppendCopies(operations, optiscaler_reference, root,
            kDlss5OptiscalerFiles);
    }
    if (use_optiscaler) {
        AppendCopies(operations, dlss5_reference, root, kDlss5Files);
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
    if (!CleanupLegacyOptiscalerPayload(root, error) ||
        (!use_optiscaler &&
            !RemoveFileIfPresent(root / L"OptiScaler.dll", error)) ||
        (!use_optiscaler &&
            !RemoveFileIfPresent(root / L"nvngx.dll_dlssnr.dll", error)) ||
        (!use_optiscaler &&
            !RemoveFileIfPresent(root /
                L"amd_fidelityfx_framegeneration_dx12.dll", error)) ||
        (!use_optiscaler &&
            !RemoveFileIfPresent(root /
                L"ofxr_amd_fidelityfx_framegeneration_dx12.dll", error)) ||
        (!use_reshade &&
            !RemoveFileIfPresent(root / kReshadeRuntime, error)) ||
        !RemoveFileIfPresent(root / L"witcher3vr_dxgi.dll", error)) {
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
