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
    const auto reshade_dlss5 =
        root / L"witcher3vr-reshade-dlss5-reference";
    const auto canonical = root / L"witcher3vr-optiscaler-reference";
    const auto modified = root / L"witcher3vr-optiscaler-dlss5-reference";
    const auto dlss5 = root / L"witcher3vr-dlss5-reference";
    WriteFile(mod / L"dxgi.dll", "mod-renderer");
    WriteFile(reshade / L"ReShade64.dll", "reshade-runtime");
    WriteFile(reshade_dlss5 / L"renodx-dlss5-v2.5.addon64",
        "ngx-only-addon");
    WriteFile(canonical / L"OptiScaler.dll", "canonical-opti");
    WriteFile(canonical / L"OptiScaler.ini",
        "[Menu]\r\nShortcutKey=0x2E\r\n");
    const std::vector<std::wstring> modified_files{
        L"OptiScaler.dll", L"OptiScaler.ini", L"nvngx.dll_dlssnr.dll"};
    for (const auto& relative : modified_files) {
        WriteFile(modified / relative,
            relative == L"OptiScaler.ini"
                ? "[Menu]\r\nShortcutKey=0x2E\r\n"
                : "modified:" + fs::path(relative).generic_string());
    }
    WriteFile(dlss5 / L"nvngx_dlss.dll", "rtx40-dlss");
    WriteFile(dlss5 / L"nvngx_dlssg.dll", "rtx40-dlssg");
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
    WriteFile(root / L"renodx-dlss5-v2.5.addon64", "obsolete-addon");
    WriteFile(root / L"CheekyFoveatedDLSS.addon64", "retired-addon");
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
    Require(ReadFile(root / L"OptiScaler.dll") == "canonical-opti",
        "Off did not publish canonical OptiScaler");
    Require(!fs::exists(root / L"ReShade64.dll") &&
            !fs::exists(root / L"renodx-dlss5-v2.5.addon64") &&
            !fs::exists(root / L"CheekyFoveatedDLSS.addon64") &&
            !fs::exists(root / L"ReShade.ini"),
        "Off left an active ReShade component");

    WriteFile(root / L"ReShade.ini",
        "[ADDON]\r\nDisabledAddons=User Addon\r\n"
        "LoadFromDllMain=user.addon64\r\n[INPUT]\r\n"
        "KeyOverlay=36,0,0,0\r\n");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Reshade, error),
        "ReShade mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"ReShade64.dll") == "reshade-runtime",
        "renderer-first ReShade chain is wrong");
    Require(!fs::exists(root / L"renodx-dlss5-v2.5.addon64"),
        "plain ReShade activated the DLSS5 add-on");
    auto ini = ReadFile(root / L"ReShade.ini");
    Require(ini.find("EnableProxyLibrary=0") != std::string::npos &&
            ini.find("ProxyLibrary=") != std::string::npos &&
            ini.find("KeyOverlay=115,0,0,0") != std::string::npos &&
            ini.find("DisabledAddons=User Addon,Generic Depth") !=
                std::string::npos &&
            ini.find("LoadFromDllMain=user.addon64") != std::string::npos &&
            ini.find("renodx-dlss5") == std::string::npos,
        "persistent ReShade settings are wrong");

    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "OptiScaler mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"OptiScaler.dll") == "canonical-opti",
        "canonical OptiScaler mode is wrong");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerReshade, error),
        "OptiScaler plus ReShade mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"ReShade64.dll") == "reshade-runtime" &&
            ReadFile(root / L"OptiScaler.dll") == "canonical-opti",
        "combined canonical mode is wrong");

    StageLegacyOptiscalerPayload(root);
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerDlss5, error),
        "OptiScaler DLSS5 mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"OptiScaler.dll") ==
                "modified:OptiScaler.dll" &&
            ReadFile(root / L"nvngx_dlss.dll") == "rtx40-dlss" &&
            ReadFile(root / L"nvngx_dlssg.dll") == "rtx40-dlssg" &&
            ReadFile(root / L"nvngx_dlssnr.dll") == "rtx40-dlssnr" &&
            ReadFile(root / L"nvngx.dll_dlssnr.dll") ==
                "modified:nvngx.dll_dlssnr.dll" &&
            !fs::exists(root / L"renodx-dlss5-v2.5.addon64") &&
            !fs::exists(root / L"OptiScaler"),
        "OptiScaler DLSS5 composition is wrong");

    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::ReshadeDlss5, error),
        "ReShade DLSS5 mode failed");
    Require(ReadFile(root / L"dxgi.dll") == "mod-renderer" &&
            ReadFile(root / L"ReShade64.dll") == "reshade-runtime" &&
            ReadFile(root / L"OptiScaler.dll") == "canonical-opti" &&
            ReadFile(root / L"renodx-dlss5-v2.5.addon64") ==
                "ngx-only-addon" &&
            !fs::exists(root / L"nvngx.dll_dlssnr.dll") &&
            !fs::exists(root / L"OptiScaler"),
        "ReShade DLSS5 composition is wrong");
    ini = ReadFile(root / L"ReShade.ini");
    Require(ini.find("LoadFromDllMain=user.addon64,"
                     "renodx-dlss5-v2.5.addon64") != std::string::npos &&
            ini.find("EnableHooks=2") != std::string::npos,
        "ReShade DLSS5 NGX-only configuration is wrong");

    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Reshade, error),
        "DLSS5 to plain ReShade transition failed");
    Require(!fs::exists(root / L"renodx-dlss5-v2.5.addon64") &&
            fs::exists(root / L"nvngx_dlss.dll"),
        "plain ReShade did not deactivate only the add-on");
    ini = ReadFile(root / L"ReShade.ini");
    Require(ini.find("LoadFromDllMain=user.addon64") != std::string::npos &&
            ini.find("LoadFromDllMain=user.addon64,") == std::string::npos,
        "plain ReShade retained the DLSS5 early-load token");

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

void TestDlss5ReferenceMustBeExact() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    fs::remove(root / L"witcher3vr-dlss5-reference" / L"nvngx_dlssnr.dll");
    std::wstring error;
    Require(!w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::ReshadeDlss5, error) &&
            !fs::exists(root / L"dxgi.dll"),
        "incomplete DLSS5 reference was accepted");
    WriteFile(root / L"witcher3vr-dlss5-reference" /
        L"nvngx_dlssnr.dll", "rtx40-dlssnr");
    WriteFile(root / L"witcher3vr-dlss5-reference" / L"OptiScaler.dll",
        "unexpected");
    Require(!w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerDlss5, error),
        "mixed DLSS5 reference was accepted");
    RequireNoStagingFiles(root);
}

void TestRepeatedModeAndStaleCleanup() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    WriteFile(root / L"ReShade.ini", "[INPUT]\r\nKeyOverlay=115,0,0,0\r\n");

    const fs::path stale_files[]{
        root / L"nvngx.dll_dlssnr.dll.w3vr-v23032-next",
        root / L"OptiScaler.dll.w3vr-v23032-next",
        root / L"ReShade.ini.w3vr-v23032-next"};
    for (const auto& path : stale_files) {
        WriteFile(path, "stale");
        Require(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY),
            "could not make stale staging fixture read-only");
    }

    const w3vr::IntegrationMode repeated_modes[]{
        w3vr::IntegrationMode::OptiscalerDlss5,
        w3vr::IntegrationMode::ReshadeDlss5,
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
            root, w3vr::IntegrationMode::ReshadeDlss5, error),
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
    std::wstring error;
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerDlss5, error),
        "first OptiScaler DLSS5 publish failed");
    Require(ReadFile(root / L"OptiScaler.ini") ==
            "[Menu]\r\nShortcutKey=0x2E\r\n",
        "missing OptiScaler.ini was not seeded from the reference");
    const std::string user =
        "[Menu]\r\nShortcutKey=0x2E\r\n[Upscalers]\r\nDx12Upscaler=dlss\r\n";
    WriteFile(root / L"OptiScaler.ini", user);
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerDlss5, error),
        "repeat OptiScaler DLSS5 publish failed");
    Require(ReadFile(root / L"OptiScaler.ini") == user,
        "OptiScaler UI settings were overwritten");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::Optiscaler, error),
        "canonical OptiScaler transition failed");
    Require(ReadFile(root / L"OptiScaler.ini") == user,
        "OptiScaler UI settings were overwritten on mode change");
    fs::remove(root / L"OptiScaler.ini");
    Require(w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::OptiscalerDlss5, error),
        "reseed after delete failed");
    Require(ReadFile(root / L"OptiScaler.ini") ==
            "[Menu]\r\nShortcutKey=0x2E\r\n",
        "deleted OptiScaler.ini was not reseeded");
    RequireNoStagingFiles(root);
}

void TestUnsafeStreamlineOverrideFailsClosed() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    WriteFile(root / L"ReShade.ini",
        "[RenoDX.DLSS5]\r\nEnableHooks=1\r\n");
    std::wstring error;
    Require(!w3vr::ApplyManagedIntegrationMode(
            root, w3vr::IntegrationMode::ReshadeDlss5, error) &&
            !fs::exists(root / L"dxgi.dll") &&
            !fs::exists(root / L"renodx-dlss5-v2.5.addon64"),
        "unsafe Streamline override was accepted");
    RequireNoStagingFiles(root);
}

void TestEveryIntegrationTransition() {
    TemporaryDirectory temporary;
    const auto& root = temporary.path;
    StageReferences(root);
    const auto references = SnapshotReferences(root);
    WriteFile(root / L"user.addon64", "unrelated");
    const auto count = static_cast<int>(w3vr::IntegrationMode::Count);
    std::wstring error;
    for (int source = 0; source < count; ++source) {
        for (int target = 0; target < count; ++target) {
            Require(w3vr::ApplyManagedIntegrationMode(root,
                static_cast<w3vr::IntegrationMode>(source), error), "source mode failed");
            const auto mode = static_cast<w3vr::IntegrationMode>(target);
            Require(w3vr::ApplyManagedIntegrationMode(root, mode, error),
                "target mode failed");
            Require(fs::exists(root / L"renodx-dlss5-v2.5.addon64") ==
                    (mode == w3vr::IntegrationMode::ReshadeDlss5) &&
                !fs::exists(root / L"CheekyFoveatedDLSS.addon64"),
                "active add-on composition is wrong");
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
        TestDlss5ReferenceMustBeExact();
        TestUnsafeStreamlineOverrideFailsClosed();
        TestRepeatedModeAndStaleCleanup();
        TestUnknownLegacyFolderEntryIsPreserved();
        TestOptiscalerIniIsPreserved();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
