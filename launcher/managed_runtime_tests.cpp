#include "managed_runtime.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        path = fs::temp_directory_path() /
            (L"w3vr-renderer-first-switch-tests-" +
                std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()));
        fs::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void WriteFile(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    Require(output.good(), "test file write failed");
}

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "test file read open failed");
    return {std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

void StageReferences(const fs::path& root) {
    const auto mod = root / L"witcher3vr-mod-reference";
    const auto reshade = root / L"witcher3vr-reshade-reference";
    const auto modified = root / L"witcher3vr-optiscaler-dlss5-reference";
    const auto dlss5 = root / L"witcher3vr-dlss5-reference";
    WriteFile(mod / L"dxgi.dll", "mod-renderer");
    WriteFile(reshade / L"ReShade64.dll", "reshade-runtime");
    const std::vector<std::wstring> modified_files{
        L"OptiScaler.dll", L"OptiScaler.ini", L"nvngx.dll_dlssnr.dll",
        L"amd_fidelityfx_framegeneration_dx12.dll",
        L"ofxr_amd_fidelityfx_framegeneration_dx12.dll"};
    for (const auto& relative : modified_files) {
        WriteFile(modified / relative,
            relative == L"OptiScaler.ini"
                ? "[Menu]\r\nShortcutKey=0x2E\r\n"
                : "modified:" + fs::path(relative).generic_string());
    }
    WriteFile(dlss5 / L"nvngx_dlssnr.dll", "rtx40-dlssnr");
}

std::vector<std::pair<fs::path, std::string>> SnapshotReferences(
    const fs::path& root) {
    std::vector<std::pair<fs::path, std::string>> result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() &&
            entry.path().wstring().find(L"-reference") != std::wstring::npos) {
            result.emplace_back(entry.path(), ReadFile(entry.path()));
        }
    }
    return result;
}

void RequireNoStagingFiles(const fs::path& root) {
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        Require(entry.path().filename().wstring().find(
                    L".w3vr-v23032-next") == std::wstring::npos,
            "temporary integration file survived");
    }
}

void StageLegacyOptiscalerPayload(const fs::path& root) {
    const std::vector<std::wstring> files{
        L"OptiScaler/amd_fidelityfx_framegeneration_dx12.dll",
        L"OptiScaler/amd_fidelityfx_loader_dx12.dll",
        L"OptiScaler/amd_fidelityfx_upscaler_dx12.dll",
        L"OptiScaler/amd_fidelityfx_vk.dll",
        L"OptiScaler/libxell.dll",
        L"OptiScaler/libxess.dll",
        L"OptiScaler/libxess_dx11.dll",
        L"OptiScaler/libxess_fg.dll",
        L"OptiScaler/D3D12_OptiScaler/D3D12Core.dll"};
    for (const auto& relative : files) {
        WriteFile(root / relative, "legacy-managed-backend");
    }
}

void TestAllModesAndReferenceImmutability() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    const auto references = SnapshotReferences(root);
    WriteFile(root / L"witcher3vr_dxgi.dll", "obsolete-reversed-chain");
    WriteFile(root / L"ReShade64.dll", "obsolete-secondary-runtime");
    WriteFile(root / L"OptiScaler.dll", "obsolete-optiscaler-runtime");
    WriteFile(root / L"renodx-dlss5-v2.5.addon64", "user-owned-addon");
    WriteFile(root / L"CheekyFoveatedDLSS.addon64", "user-owned-addon");
    WriteFile(root / L"amd_fidelityfx_dx12.dll", "game-owned-ffx");
    WriteFile(root / L"amd_fidelityfx_upscaler_dx12.dll",
        "game-owned-upscaler");

    std::wstring error;
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Off, error),
        "Off mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            !fs::exists(root / L"witcher3vr_dxgi.dll"),
        "Off did not publish only the renderer proxy");
    Require(!fs::exists(root / L"OptiScaler.dll"),
        "Off left the OptiScaler runtime active");
    Require(!fs::exists(root / L"ReShade64.dll") &&
            ReadFile(root / L"renodx-dlss5-v2.5.addon64") ==
                "user-owned-addon" &&
            ReadFile(root / L"CheekyFoveatedDLSS.addon64") ==
                "user-owned-addon" &&
            !fs::exists(root / L"ReShade.ini"),
        "Off touched a user-owned add-on or left the ReShade runtime active");

    WriteFile(root / L"ReShade.ini",
        "[ADDON]\r\nDisabledAddons=User Addon\r\n"
        "LoadFromDllMain=user.addon64,renodx-dlss5-v2.5.addon64\r\n[INPUT]\r\n"
        "KeyOverlay=36,0,0,0\r\n");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Reshade, error),
        "ReShade mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"ReShade64.dll") == "reshade-runtime" &&
            !fs::exists(root / L"OptiScaler.dll"),
        "renderer-first ReShade chain is wrong");
    Require(ReadFile(root / L"renodx-dlss5-v2.5.addon64") ==
            "user-owned-addon",
        "plain ReShade touched the user-owned DLSS5 add-on");
    auto ini = ReadFile(root / L"ReShade.ini");
    Require(ini == "[ADDON]\r\nDisabledAddons=User Addon\r\n"
            "LoadFromDllMain=user.addon64,renodx-dlss5-v2.5.addon64\r\n"
            "[INPUT]\r\nKeyOverlay=36,0,0,0\r\n",
        "ReShade settings were modified");

    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "OptiScaler mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"OptiScaler.dll") ==
                "modified:OptiScaler.dll" &&
            ReadFile(root / L"nvngx_dlssnr.dll") == "rtx40-dlssnr" &&
            !fs::exists(root / L"nvngx_dlss.dll") &&
            !fs::exists(root / L"nvngx_dlssg.dll"),
        "OptiScaler mode did not use the lean NR payload");
    WriteFile(root / L"OptiScaler.ini",
        "; persistent user file\r\n[Log]\r\nLogToFile=true\r\n");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerReshade, error),
        "OptiScaler plus ReShade mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"ReShade64.dll") == "reshade-runtime" &&
            ReadFile(root / L"OptiScaler.dll") ==
                "modified:OptiScaler.dll",
        "combined VR OptiScaler mode is wrong");

    StageLegacyOptiscalerPayload(root);
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "OptiScaler repeat mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"OptiScaler.dll") ==
                "modified:OptiScaler.dll" &&
            ReadFile(root / L"nvngx_dlssnr.dll") == "rtx40-dlssnr" &&
            ReadFile(root / L"nvngx.dll_dlssnr.dll") ==
                "modified:nvngx.dll_dlssnr.dll" &&
            ReadFile(root / L"amd_fidelityfx_framegeneration_dx12.dll") ==
                "modified:amd_fidelityfx_framegeneration_dx12.dll" &&
            ReadFile(root / L"ofxr_amd_fidelityfx_framegeneration_dx12.dll") ==
                "modified:ofxr_amd_fidelityfx_framegeneration_dx12.dll" &&
            ReadFile(root / L"OptiScaler.ini").find(
                "; persistent user file") != std::string::npos &&
            ReadFile(root / L"renodx-dlss5-v2.5.addon64") ==
                "user-owned-addon" &&
            !fs::exists(root / L"OptiScaler"),
        "OptiScaler composition is wrong");

    for (const auto& [path, contents] : references) {
        Require(fs::exists(path) && ReadFile(path) == contents,
            "a reference source was modified");
    }
    Require(ReadFile(root / L"amd_fidelityfx_dx12.dll") ==
                "game-owned-ffx" &&
            ReadFile(root / L"amd_fidelityfx_upscaler_dx12.dll") ==
                "game-owned-upscaler",
        "game-owned FidelityFX runtime was modified");
    RequireNoStagingFiles(root);
}

void TestDlss5ReferenceNeedsOnlyNr() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    fs::remove(root / L"witcher3vr-dlss5-reference" / L"nvngx_dlssnr.dll");
    std::wstring error;
    Require(!w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error) &&
            !fs::exists(root / L"dxgi.dll"),
        "incomplete DLSS5 reference was accepted");
    WriteFile(root / L"witcher3vr-dlss5-reference" /
        L"nvngx_dlssnr.dll", "rtx40-dlssnr");
    WriteFile(root / L"witcher3vr-dlss5-reference" / L"OptiScaler.dll",
        "unexpected");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error) &&
            !fs::exists(root / L"nvngx_dlss.dll") &&
            !fs::exists(root / L"nvngx_dlssg.dll"),
        "extra reference files should be ignored");
    RequireNoStagingFiles(root);
}

void TestRepeatedModeAndStaleCleanup() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    WriteFile(root / L"ReShade.ini", "[INPUT]\r\nKeyOverlay=115,0,0,0\r\n");

    const fs::path stale_files[]{
        root / L"nvngx.dll_dlssnr.dll.w3vr-v23032-next",
        root / L"OptiScaler.dll.w3vr-v23032-next"};
    for (const auto& path : stale_files) {
        WriteFile(path, "stale");
        Require(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY),
            "could not make stale staging fixture read-only");
    }

    const w3vr::IntegrationMode repeated_modes[]{
        w3vr::IntegrationMode::Optiscaler,
        w3vr::IntegrationMode::Reshade,
        w3vr::IntegrationMode::Off};
    std::wstring error;
    for (const auto mode : repeated_modes) {
        for (int pass = 0; pass < 2; ++pass) {
            Require(w3vr::ApplyManagedIntegrationMode(root, mode, error),
                "repeated unchanged integration mode failed");
        }
    }
    RequireNoStagingFiles(root);
}

void TestUnknownLegacyFolderEntryIsPreserved() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    StageLegacyOptiscalerPayload(root);
    WriteFile(root / L"OptiScaler/user-owned.keep", "do-not-delete");
    std::wstring error;
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Reshade, error),
        "legacy payload cleanup with unknown entry failed");
    Require(ReadFile(root / L"OptiScaler/user-owned.keep") ==
                "do-not-delete" &&
            !fs::exists(root / L"OptiScaler/libxess.dll"),
        "legacy cleanup removed an unknown entry or retained a managed file");
    RequireNoStagingFiles(root);
}

void TestOptiscalerIniIsPreserved() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    const std::string user =
        "[Menu]\r\nShortcutKey=0x2E\r\n[Upscalers]\r\nDx12Upscaler=dlss\r\n";
    WriteFile(root / L"OptiScaler.ini", user);
    std::wstring error;
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "OptiScaler publish failed");
    Require(ReadFile(root / L"OptiScaler.ini") == user,
        "OptiScaler UI settings were overwritten");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "canonical OptiScaler transition failed");
    Require(ReadFile(root / L"OptiScaler.ini") == user,
        "OptiScaler UI settings were overwritten on mode change");
    fs::remove(root / L"OptiScaler.ini");
    Require(!fs::exists(root / L"OptiScaler.ini"),
        "deleted OptiScaler.ini was unexpectedly recreated");
    RequireNoStagingFiles(root);
}

void TestEveryIntegrationTransition() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    const auto references = SnapshotReferences(root);
    WriteFile(root / L"user.addon64", "unrelated");
    WriteFile(root / L"renodx-dlss5-v2.5.addon64", "user-owned");
    const auto count = static_cast<int>(w3vr::IntegrationMode::Count);
    std::wstring error;
    for (int source = 0; source < count; ++source) {
        for (int target = 0; target < count; ++target) {
            Require(w3vr::ApplyManagedIntegrationMode(root,
                static_cast<w3vr::IntegrationMode>(source), error), "source mode failed");
            const auto mode = static_cast<w3vr::IntegrationMode>(target);
            Require(w3vr::ApplyManagedIntegrationMode(root, mode, error),
                "target mode failed");
            Require(ReadFile(root / L"renodx-dlss5-v2.5.addon64") ==
                    "user-owned",
                "integration transition touched a user-owned add-on");
        }
    }
    for (const auto& [path, contents] : references) {
        Require(ReadFile(path) == contents, "transition modified a reference");
    }
    Require(ReadFile(root / L"user.addon64") == "unrelated",
        "transition touched an unmanaged add-on");
    RequireNoStagingFiles(root);
}

} // namespace

int main() {
    try {
        TestAllModesAndReferenceImmutability();
        TestEveryIntegrationTransition();
        TestDlss5ReferenceNeedsOnlyNr();
        TestRepeatedModeAndStaleCleanup();
        TestUnknownLegacyFolderEntryIsPreserved();
        TestOptiscalerIniIsPreserved();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
