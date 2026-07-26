#include "config.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    Require(static_cast<bool>(stream), "fixture write failed");
}

std::string Read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(stream)), {}};
}

struct TempDirectory {
    fs::path path;
    TempDirectory() {
        wchar_t root[MAX_PATH]{};
        GetTempPathW(MAX_PATH, root);
        path = fs::path(root) / (L"w3vr-launcher-tests-" +
            std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDirectory() { fs::remove_all(path); }
};

w3vr::ConfigPaths MakePaths(const fs::path& root) {
    return {root, root / "witcher3vr.ini", root / "dx12user.settings",
        root / "witcher3.exe"};
}

void WriteBaseFixtures(const w3vr::ConfigPaths& paths) {
    Write(paths.vr_ini,
        "; preserve this comment\r\n"
        "[openxr]\r\n"
        "enabled=1\r\n"
        "mode=4\r\n"
        "render_width=2688\r\n"
        "render_height=2784\r\n"
        "hud_stereo_shift_px=-16\r\n"
        "hud_size=1.000\r\n"
        "presentation_scale=0.900\r\n"
        "hud_horizontal_scale=0.500\r\n"
        "hud_vertical_scale=0.500\r\n"
        "menu_scale=0.900\r\n"
        "cinema_5x4=0\r\n"
        "vertical_pitch_enabled=0\r\n"
        "hmd_position_scale=0.375\r\n"
        "untouched_openxr=alpha\r\n"
        "\r\n"
        "[engine]\r\n"
        "temporal_backend=dlss_packed\r\n"
        "dlss_dlaa=0\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "close_camera_offset=0.750\r\n"
        "unrelated_engine=42\r\n"
        "\r\n"
        "[debug]\r\n"
        "logging_enabled=0\r\n"
        "runtime_diagnostics=0\r\n"
        "unrelated_debug=keep\r\n");
    Write(paths.game_settings,
        "[Viewport]\r\n"
        "Resolution=\"2688x2784\"\r\n"
        "VSync=false\r\n"
        "[PostProcess]\r\n"
        "AAMode=6\r\n"
        "DLSSQuality=3\r\n"
        "SharpenAmount=0.5\r\n"
        "[Rendering]\r\n"
        "AllowDLSS=true\r\n"
        "TextureQuality=Ultra\r\n"
        "[Galaxy]\r\n"
        "tokenRefA=user-secret\r\n");
}

void TestAllModes(const w3vr::ConfigPaths& paths) {
    for (int index = 0; index < 6; ++index) {
        WriteBaseFixtures(paths);
        w3vr::LauncherState state;
        state.mode = static_cast<w3vr::RenderMode>(index);
        state.width = 2496 + index;
        state.height = 2592 + index;
        state.dlss_quality = index % 5;
        state.hud_convergence_delta = 7;
        state.presentation_scale = 0.85f;
        state.hud_horizontal_scale = 0.25f;
        state.hud_vertical_scale = 0.30f;
        state.menu_scale = 0.75f;
        state.cinema_scale = 1.1f;
        state.near_view = 1.25f;
        state.vertical_pitch_enabled = true;
        state.cinema_5x4 = true;
        state.diagnostic_logging = true;

        w3vr::IniDocument vr;
        w3vr::IniDocument game;
        std::wstring error;
        Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error),
            "mode document build failed");
        const auto expected = w3vr::SettingsForMode(state.mode);
        Require(vr.Get("openxr", "mode") == std::to_string(expected.openxr_mode),
            "wrong OpenXR mode");
        Require(vr.Get("engine", "temporal_backend") == expected.temporal_backend,
            "wrong temporal backend");
        const bool expected_dlaa =
            w3vr::ModeUsesDlss(state.mode) && state.dlss_quality == 0;
        Require(vr.Get("engine", "dlss_dlaa") ==
            std::string(expected_dlaa ? "1" : "0"), "wrong DLAA flag");
        Require(vr.Get("engine", "dual_render_probe") ==
            std::string(expected.dual_render ? "1" : "0"), "wrong dual probe");
        Require(vr.Get("engine", "dual_render_start") ==
            std::string(expected.dual_render ? "1" : "0"), "wrong dual start");
        Require(game.Get("PostProcess", "AAMode") == std::to_string(expected.aa_mode),
            "wrong AA mode");
        Require(game.Get("Rendering", "AllowDLSS") ==
            std::string(expected.allow_dlss ? "true" : "false"), "wrong DLSS flag");
        Require(game.Get("PostProcess", "DLSSQuality") ==
            std::to_string(state.dlss_quality == 0 ? 1 : state.dlss_quality),
            "wrong DLSS bootstrap quality");
        Require(game.Get("Viewport", "Resolution") == "\"" +
            std::to_string(state.width) + "x" + std::to_string(state.height) + "\"",
            "wrong game resolution");
        Require(vr.Get("openxr", "hud_stereo_shift_px") == "-9",
            "wrong zero-relative convergence conversion");
        Require(vr.Get("openxr", "presentation_scale") == "0.850",
            "presentation scale missing");
        Require(vr.Get("openxr", "hud_horizontal_scale") == "0.250",
            "HUD X scale missing");
        Require(vr.Get("openxr", "hud_vertical_scale") == "0.300",
            "HUD Y scale missing");
        Require(vr.Get("openxr", "hud_size") == "1.000",
            "obsolete HUD size should remain untouched");
        Require(vr.Get("openxr", "hmd_position_scale") == "0.375",
            "launcher must not modify unmanaged HMD position scale");
        Require(vr.Get("openxr", "cinema_scale") == "1.100",
            "cinema scale missing");
        Require(vr.Get("openxr", "cinema_5x4") == "1",
            "extended cinema framing flag missing");
        Require(vr.Get("debug", "logging_enabled") == "1",
            "diagnostic log writer flag missing");
        Require(vr.Get("debug", "runtime_diagnostics") == "1",
            "runtime diagnostics flag missing");
        Require(vr.Get("debug", "unrelated_debug") == "keep",
            "unrelated debug setting not preserved");
        Require(vr.Get("engine", "close_camera_offset") == "1.250",
            "near view missing");
        Require(vr.Serialize().find("; preserve this comment") != std::string::npos,
            "comment not preserved");
        Require(vr.Get("engine", "unrelated_engine") == "42",
            "unrelated VR setting not preserved");
        Require(game.Get("Rendering", "TextureQuality") == "Ultra",
            "unrelated game setting not preserved");

        Write(paths.vr_ini, vr.Serialize());
        Write(paths.game_settings, game.Serialize());
        const auto loaded = w3vr::LoadConfiguration(paths);
        Require(loaded.warning.empty(), "saved mode should infer exactly");
        Require(loaded.state.mode == state.mode, "round-trip mode mismatch");
        Require(loaded.state.width == state.width && loaded.state.height == state.height,
            "round-trip resolution mismatch");
        if (w3vr::ModeUsesDlss(state.mode)) {
            Require(loaded.state.dlss_quality == state.dlss_quality,
                "round-trip DLSS/DLAA selection mismatch");
        }
        Require(loaded.state.cinema_5x4,
            "round-trip extended cinema framing mismatch");
        Require(loaded.state.diagnostic_logging,
            "round-trip diagnostic logging mismatch");
    }
}

void TestDlssLabelsAndLegacyAuto(const w3vr::ConfigPaths& paths) {
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::MonoNone)) ==
        L"Mono - No AA / FXAA", "mono No AA / FXAA label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::StereoNone)) ==
        L"Stereo - No AA / FXAA", "stereo No AA / FXAA label mismatch");

    WriteBaseFixtures(paths);
    std::wstring error;
    auto game = w3vr::IniDocument::Load(paths.game_settings, error);
    Require(game.has_value(), "legacy Auto fixture load failed");
    game->Set("PostProcess", "DLSSQuality", "0");
    Write(paths.game_settings, game->Serialize());
    const auto loaded = w3vr::LoadConfiguration(paths);
    Require(loaded.state.dlss_quality == 1,
        "legacy Auto must display as supported Quality instead of DLAA");

    auto vr = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(vr.has_value(), "partial diagnostics fixture load failed");
    vr->Set("debug", "logging_enabled", "1");
    vr->Set("debug", "runtime_diagnostics", "0");
    Write(paths.vr_ini, vr->Serialize());
    Require(!w3vr::LoadConfiguration(paths).state.diagnostic_logging,
        "partial manual diagnostics must not display as a full diagnostic run");
}

void TestFallbackAndAtomicSave(const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    auto text = Read(paths.vr_ini);
    const auto cinema_missing = w3vr::LoadConfiguration(paths);
    Require(cinema_missing.state.cinema_scale == cinema_missing.state.menu_scale,
        "cinema fallback must match menu scale");

    w3vr::LauncherState state;
    state.mode = w3vr::RenderMode::MonoTaau;
    state.width = 3072;
    state.height = 3216;
    std::wstring error;
    Require(w3vr::SaveConfiguration(paths, state, error), "atomic save failed");
    Require(fs::exists(paths.vr_ini.wstring() + L".w3vr.bak"),
        "VR backup was not created");
    Require(fs::exists(paths.game_settings.wstring() + L".w3vr.bak"),
        "game backup was not created");
    Require(Read(paths.vr_ini.wstring() + L".w3vr.bak") == text,
        "VR backup does not contain the previous file");
}

void TestInconsistentWarning(const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    std::wstring error;
    auto game = w3vr::IniDocument::Load(paths.game_settings, error);
    Require(game.has_value(), "game fixture load failed");
    game->Set("PostProcess", "AAMode", "0");
    Write(paths.game_settings, game->Serialize());
    const auto loaded = w3vr::LoadConfiguration(paths);
    Require(!loaded.warning.empty(), "inconsistent settings should warn");
    Require(loaded.state.mode == w3vr::RenderMode::StereoDlssSequential,
        "best-effort mode should follow VR backend");
}

void TestFirstRunConfiguration(const fs::path& root) {
    const auto paths = MakePaths(root / "first-run");
    fs::create_directories(paths.launcher_directory);
    bool created{};
    std::wstring error;
    const std::string defaults =
        "[openxr]\r\n"
        "mode=4\r\n"
        "render_width=2688\r\n"
        "render_height=2784\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "[debug]\r\n"
        "logging_enabled=0\r\n"
        "runtime_diagnostics=0\r\n";
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "first-run INI creation failed");
    Require(created, "first-run INI was not reported as created");
    Require(Read(paths.vr_ini) == defaults,
        "first-run INI does not match embedded defaults");

    Write(paths.vr_ini, "[openxr]\r\nmode=3\r\n");
    created = true;
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "existing INI check failed");
    Require(!created, "existing INI must not be replaced");
    Require(Read(paths.vr_ini) == "[openxr]\r\nmode=3\r\n",
        "existing INI was overwritten");
}

void TestVrBaselineAndRestore(const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    const std::string original = Read(paths.game_settings);
    const std::string profile =
        "[Viewport]\r\n"
        "Resolution=\"2688x2784\"\r\n"
        "[Rendering]\r\n"
        "GrassDensity=3500\r\n"
        "[Galaxy]\r\n"
        "tokenRefA=\r\n";
    std::wstring error;
    Require(w3vr::ConfigureGameSettingsForVr(paths, profile, error),
        "VR baseline install failed");
    Require(w3vr::HasOriginalSettingsBackup(paths),
        "original backup should enable restore");
    Require(Read(w3vr::OriginalSettingsBackupPath(paths)) == original,
        "original backup contents changed");
    auto configured = w3vr::IniDocument::Load(paths.game_settings, error);
    Require(configured.has_value(), "configured settings could not be read");
    Require(configured->Get("Rendering", "GrassDensity") == "3500",
        "complete VR profile was not installed");
    Require(configured->Get("Galaxy", "tokenRefA") == "user-secret",
        "target account data was not preserved");

    Require(w3vr::ConfigureGameSettingsForVr(paths, profile, error),
        "repeated VR baseline install failed");
    Require(Read(w3vr::OriginalSettingsBackupPath(paths)) == original,
        "repeated configure overwrote the original backup");
    Require(w3vr::RestoreOriginalGameSettings(paths, error),
        "original settings restore failed");
    Require(Read(paths.game_settings) == original,
        "restore did not reproduce the original file exactly");
    Require(!w3vr::HasOriginalSettingsBackup(paths),
        "restored backup should be consumed and disable the button");
}

void TestFailurePaths(const fs::path& root) {
    const auto missing = MakePaths(root / "missing");
    w3vr::IniDocument vr;
    w3vr::IniDocument game;
    std::wstring error;
    Require(!w3vr::BuildUpdatedDocuments(
        missing, w3vr::LauncherState{}, vr, game, error),
        "missing source files should fail");
    Require(!error.empty(), "missing source failure should explain itself");

    error.clear();
    Require(!w3vr::AtomicWriteWithBackup(
        root / "nonexistent-directory" / "settings.ini", "x=1", error),
        "write into a missing directory should fail");
    Require(!error.empty(), "write failure should explain itself");
}

} // namespace

int main() {
    try {
        TempDirectory temporary;
        const auto paths = MakePaths(temporary.path);
        TestAllModes(paths);
        TestDlssLabelsAndLegacyAuto(paths);
        TestFallbackAndAtomicSave(paths);
        TestInconsistentWarning(paths);
        TestFirstRunConfiguration(temporary.path);
        TestVrBaselineAndRestore(paths);
        TestFailurePaths(temporary.path);
        std::cout << "All launcher configuration tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return 1;
    }
}
