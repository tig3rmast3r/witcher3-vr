#include "config.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

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

std::string Utf16Bytes(std::u16string_view text, bool little_endian = true) {
    std::string bytes;
    bytes.reserve(2 + text.size() * 2);
    bytes.push_back(little_endian ? static_cast<char>(0xFF) :
        static_cast<char>(0xFE));
    bytes.push_back(little_endian ? static_cast<char>(0xFE) :
        static_cast<char>(0xFF));
    for (const char16_t value : text) {
        const char low = static_cast<char>(value & 0xFF);
        const char high = static_cast<char>((value >> 8) & 0xFF);
        bytes.push_back(little_endian ? low : high);
        bytes.push_back(little_endian ? high : low);
    }
    return bytes;
}

size_t CountOccurrences(const std::string& text, const std::string& token) {
    size_t count{};
    size_t position{};
    while ((position = text.find(token, position)) != std::string::npos) {
        ++count;
        position += token.size();
    }
    return count;
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
        "fullscreen_projection=1\r\n"
        "hud_horizontal_scale=0.500\r\n"
        "hud_vertical_scale=0.500\r\n"
        "menu_scale=0.900\r\n"
        "menu_distance=1.200\r\n"
        "cinema_render_stereo_strength=0.250\r\n"
        "cinema_hud_stereo_shift_px=-36\r\n"
        "cinema_hud_scale=1.300\r\n"
        "manual_cinema_hud_scale=1.600\r\n"
        "full_vr_hud_stereo_shift_px=-192\r\n"
        "full_vr_hud_scale=0.750\r\n"
        "cinema_5x4=0\r\n"
        "cinema_full_vr=0\r\n"
        "steady_icons=0\r\n"
        "vertical_pitch_enabled=0\r\n"
        "hmd_position_scale=0.375\r\n"
        "snap_turn_enabled=1\r\n"
        "snap_turn_angle=30\r\n"
        "hud_convergence_offset_px=24\r\n"
        "cinematic_16_9=1\r\n"
        "cinema_subtitle_scale=1.300\r\n"
        "full_vr_subtitle_scale=0.800\r\n"
        "cinema_subtitle_stereo_shift_px=-88\r\n"
        "untouched_openxr=alpha\r\n"
        "\r\n"
        "[engine]\r\n"
        "temporal_backend=dlss_packed\r\n"
        "dlss_dlaa=0\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "close_camera_offset=0.750\r\n"
        "first_person_snap_turn=0\r\n"
        "first_person_snap_turn_degrees=45\r\n"
        "first_person_hmd_body_follow=0\r\n"
        "first_person_combat_exit=0\r\n"
        "first_person_stationary_turn=1\r\n"
        "streamline_ps93_learning_log=1\r\n"
        "unrelated_engine=42\r\n"
        "\r\n"
        "[reverse]\r\n"
        "enabled=1\r\n"
        "scan_periodic=0\r\n"
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
        "[DLC]\r\n"
        "DlcEnabled_movementinputfix=0\r\n"
        "DlcEnabled_unrelated=0\r\n"
        "[Localization]\r\n"
        "SpeechLanguage=DE\r\n"
        "TextLanguage=DE\r\n"
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
        state.menu_scale = 0.75f;
        state.cinema_scale = 1.1f;
        state.cinema_hud_scale = 1.5f;
        state.cinema_hud_convergence_offset = 7;
        state.full_vr_hud_scale = 1.25f;
        state.full_vr_hud_convergence_offset = -9;
        state.near_view = 1.25f;
        state.vertical_pitch_enabled = true;
        state.cinema_full_vr = true;
        state.steady_icons = true;
        state.first_person_gamepad_head_follow = true;
        state.first_person_snap_turn_degrees = 60;
        state.first_person_combat_exit = true;
        state.first_person_stationary_turn = index % 2 != 0;
        state.fast_movement_transitions = index % 2 == 0;
        state.native_stereo = true;
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
        Require(game.Get("DLC", "DlcEnabled_movementinputfix") ==
            std::string(state.fast_movement_transitions ? "1" : "0"),
            "wrong faster-transitions DLC flag");
        Require(game.Get("DLC", "DlcEnabled_unrelated") == "0",
            "unrelated DLC flag not preserved");
        Require(vr.Get("openxr", "hud_stereo_shift_px") == "-9",
            "wrong zero-relative convergence conversion");
        Require(vr.Get("openxr", "presentation_scale") ==
            std::string(state.mode == w3vr::RenderMode::StereoNone
                ? "1.000" : "0.850"),
            "native stereo presentation-scale contract mismatch");
        Require(vr.Get("openxr", "fullscreen_projection") ==
            std::string(state.mode == w3vr::RenderMode::StereoNone ? "1" : "0"),
            "native stereo must be limited to Stereo No AA");
        Require(vr.Get("openxr", "hud_horizontal_scale") == "0.500",
            "removed HUD X control must preserve the existing INI value");
        Require(vr.Get("openxr", "hud_vertical_scale") == "0.500",
            "removed HUD Y control must preserve the existing INI value");
        Require(vr.Get("openxr", "hud_size") == "1.000",
            "unmanaged HUD size should remain untouched");
        Require(vr.Get("openxr", "hmd_position_scale") == "0.375",
            "launcher must not modify unmanaged HMD position scale");
        Require(!vr.Get("openxr", "snap_turn_enabled").has_value() &&
            !vr.Get("openxr", "snap_turn_angle").has_value(),
            "legacy OpenXR snap-turn keys were not removed");
        Require(!vr.Get("openxr", "hud_convergence_offset_px").has_value() &&
            !vr.Get("openxr", "cinematic_16_9").has_value() &&
            !vr.Get("openxr", "cinema_subtitle_scale").has_value() &&
            !vr.Get("openxr", "full_vr_subtitle_scale").has_value() &&
            !vr.Get("openxr", "cinema_subtitle_stereo_shift_px").has_value(),
            "obsolete OpenXR trial keys were not removed");
        Require(!vr.Get("engine", "streamline_ps93_learning_log").has_value(),
            "obsolete engine learning-log key was not removed");
        Require(vr.Get("openxr", "cinema_scale") == "1.100",
            "cinema scale missing");
        Require(vr.Get("openxr", "menu_distance") == "1.200",
            "launcher must not modify menu distance");
        Require(vr.Get("openxr", "cinema_render_stereo_strength") == "0.250",
            "launcher must preserve unmanaged cinema render strength");
        Require(vr.Get("openxr", "cinema_hud_scale") == "1.500" &&
            vr.Get("openxr", "cinema_hud_stereo_shift_px") == "-76",
            "Cinema3D HUD scale/automatic convergence mismatch");
        Require(vr.Get("openxr", "full_vr_hud_scale") == "1.250" &&
            vr.Get("openxr", "full_vr_hud_stereo_shift_px") == "-38",
            "Full VR HUD scale/automatic convergence mismatch");
        Require(vr.Get("openxr", "manual_cinema_hud_scale") == "1.600",
            "manual F10 HUD scale must remain independently tuned");
        Require(vr.Get("openxr", "cinema_5x4") == "1",
            "fixed cinema framing flag missing");
        Require(vr.Get("meta", "config_version") == "6",
            "configuration version marker missing");
        Require(vr.Get("openxr", "cinema_full_vr") == "1",
            "automatic full-VR cutscene flag missing");
        Require(vr.Get("openxr", "steady_icons") == "1",
            "steady-icons latency flag missing");
        Require(vr.Get("engine", "first_person_snap_turn") == "1",
            "first-person snap-turn flag missing");
        Require(vr.Get("engine", "first_person_hmd_body_follow") == "1",
            "first-person HMD body-follow flag missing");
        Require(vr.Get("engine", "first_person_snap_turn_degrees") == "60",
            "first-person snap-turn angle missing");
        Require(vr.Get("engine", "first_person_combat_exit") == "1",
            "first-person combat-exit flag missing");
        Require(vr.Get("engine", "first_person_stationary_turn") ==
            std::string(state.first_person_stationary_turn ? "1" : "0"),
            "first-person stationary-turn flag missing");
        Require(vr.Get("debug", "logging_enabled") == "1",
            "diagnostic log writer flag missing");
        Require(vr.Get("debug", "runtime_diagnostics") == "1",
            "runtime diagnostics flag missing");
        Require(vr.Get("debug", "taau_drop_diagnostics") == "1",
            "per-eye TAAU diagnostics flag missing");
        Require(vr.Get("debug", "cinema_camera_diagnostics") == "1" &&
            vr.Get("debug", "cinema_subtitle_diagnostics") == "1" &&
            vr.Get("debug", "first_person_state_diagnostics") == "1" &&
            vr.Get("debug", "first_person_aim_diagnostics") == "1" &&
            vr.Get("debug", "world_marker_diagnostics") == "1",
            "diagnostic checkbox does not own every runtime probe");
        Require(vr.Get("reverse", "enabled") == "0",
            "launcher must clear the obsolete reverse master workaround");
        Require(vr.Get("reverse", "scan_periodic") == "0",
            "launcher must preserve individual reverse probe settings");
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
        Require(loaded.state.native_stereo ==
            (state.mode == w3vr::RenderMode::StereoNone),
            "round-trip native stereo mode gate mismatch");
        if (w3vr::ModeUsesDlss(state.mode)) {
            Require(loaded.state.dlss_quality == state.dlss_quality,
                "round-trip DLSS/DLAA selection mismatch");
        }
        Require(loaded.state.cinema_full_vr,
            "round-trip automatic full-VR cutscene mismatch");
        Require(loaded.state.steady_icons,
            "round-trip steady-icons latency mismatch");
        Require(loaded.state.first_person_gamepad_head_follow,
            "round-trip first-person gamepad head-follow mismatch");
        Require(loaded.state.first_person_snap_turn_degrees == 60,
            "round-trip first-person snap-turn angle mismatch");
        Require(loaded.state.first_person_combat_exit,
            "round-trip first-person combat-exit mismatch");
        Require(loaded.state.first_person_stationary_turn ==
            state.first_person_stationary_turn,
            "round-trip first-person stationary-turn mismatch");
        Require(loaded.state.cinema_hud_scale == state.cinema_hud_scale &&
            loaded.state.cinema_hud_convergence_offset ==
                state.cinema_hud_convergence_offset,
            "round-trip Cinema3D HUD tuning mismatch");
        Require(loaded.state.full_vr_hud_scale == state.full_vr_hud_scale &&
            loaded.state.full_vr_hud_convergence_offset ==
                state.full_vr_hud_convergence_offset,
            "round-trip Full VR HUD tuning mismatch");
        Require(loaded.state.fast_movement_transitions ==
            state.fast_movement_transitions,
            "round-trip faster-transitions DLC mismatch");
        Require(loaded.state.diagnostic_logging,
            "round-trip diagnostic logging mismatch");
    }
}

void TestProportionalCutsceneConvergence() {
    Require(w3vr::CinemaHudConvergenceShift(1.30f, 0) == -72,
        "Cinema3D reference convergence changed");
    Require(w3vr::CinemaHudConvergenceShift(0.65f, 0) == -36,
        "Cinema3D convergence does not follow HUD scale");
    Require(w3vr::CinemaHudConvergenceShift(1.30f, 10) == -62,
        "Cinema3D manual offset was not added after the automatic base");
    Require(w3vr::FullVrHudConvergenceShift(1.00f, 0) == -36,
        "Full VR reference convergence changed");
    Require(w3vr::FullVrHudConvergenceShift(1.50f, 0) == -24,
        "Full VR physical depth changed with HUD scale");
    Require(w3vr::FullVrHudConvergenceShift(1.00f, 100) == 28,
        "cutscene convergence offset must remain bounded");
}

void TestReleaseDefaults() {
    const w3vr::LauncherState defaults;
    Require(defaults.full_vr_hud_scale == 1.0f &&
        w3vr::FullVrHudConvergenceShift(
            defaults.full_vr_hud_scale,
            defaults.full_vr_hud_convergence_offset) == -36,
        "Full VR cutscene HUD must default to size 1.0 at gameplay depth");
    Require(defaults.cinema_full_vr,
        "automatic Full VR cutscenes must default to enabled");
    Require(!defaults.steady_icons,
        "steady icons must default to disabled");
    Require(!defaults.vertical_pitch_enabled,
        "vertical mouse/pad pitch must default to disabled");
    Require(defaults.first_person_combat_exit,
        "automatic combat camera switch must default to enabled");
    Require(!defaults.first_person_stationary_turn,
        "stationary first-person body turn must default to disabled");
    Require(defaults.fast_movement_transitions,
        "faster movement transitions must default to enabled");
    Require(!defaults.native_stereo,
        "experimental native stereo must default to disabled");
    Require(!defaults.diagnostic_logging,
        "diagnostic logging must default to disabled");
}

void TestHudEditorSetup(const fs::path& root) {
    const fs::path game = root / "hud-setup" / "game";
    const fs::path launcher_directory = game / "bin" / "x64_dx12";
    const fs::path documents = root / "hud-setup" / "Documents" /
        "The Witcher 3";
    const w3vr::ConfigPaths paths{
        launcher_directory,
        launcher_directory / "witcher3vr.ini",
        documents / "dx12user.settings",
        launcher_directory / "witcher3.exe"};
    const fs::path script = game / "mods" / "modWitcher3VRHUDEditor" /
        "content" / "scripts" / "local" / "witcher3vr_hud_editor" /
        "hud_editor.ws";
    const fs::path config_directory = game / "bin" / "config" / "r4game" /
        "user_config_matrix" / "pc";
    const fs::path xml = config_directory / "modWitcher3VRHUDEditor.xml";
    const fs::path filelist = config_directory / "dx12filelist.txt";
    const fs::path input = documents / "input.settings";

    Write(script, "// fixture\n");
    Write(xml, "<UserConfig/>\n");
    Write(filelist, "audio.xml;\ninput.xml;\n");
    Write(input,
        "[Exploration]\r\n"
        "IK_F12=(Action=UnrelatedAction)\r\n"
        "\r\n"
        "[W3VRHudEditor]\r\n"
        "IK_Left=(Action=W3VRHudEditorPrevious)\r\n"
        "IK_Right=(Action=W3VRHudEditorNext)\r\n"
        "IK_A=(Action=W3VRHudEditorMoveX,State=Axis,Value=-1)\r\n"
        "IK_LeftMouse=(Action=W3VRHudEditorDrag)\r\n"
        "IK_Escape=(Action=W3VRHudEditorExit)\r\n"
        "IK_Tab=(Action=W3VRHudEditorProfile)\r\n"
        "CustomBinding=keep\r\n"
        "\r\n"
        "[Unrelated]\r\n"
        "Value=preserve\r\n");

    std::wstring error;
    Require(w3vr::EnsureHudEditorSetup(paths, error),
        "HUD editor setup failed");
    const std::string first_filelist = Read(filelist);
    const std::string first_input = Read(input);
    Require(CountOccurrences(first_filelist,
        "modWitcher3VRHUDEditor.xml;") == 1,
        "HUD config XML was not registered exactly once");
    Require(CountOccurrences(first_input,
        "Action=W3VRHudEditorToggle") == 13,
        "HUD editor toggle was not installed in every supported context");
    Require(first_input.find("IK_Q=(Action=W3VRHudEditorPrevious)") !=
            std::string::npos &&
        first_input.find("IK_E=(Action=W3VRHudEditorNext)") !=
            std::string::npos &&
        first_input.find("IK_Left=(Action=W3VRHudEditorMoveLeft)") !=
            std::string::npos &&
        first_input.find("IK_Right=(Action=W3VRHudEditorMoveRight)") !=
            std::string::npos &&
        first_input.find("IK_F7=(Action=W3VRHudEditorProfile)") !=
            std::string::npos,
        "validated HUD editor controls were not installed");
    Require(CountOccurrences(first_input,
            "Action=W3VRHudEditorProfile") == 13,
        "global HUD profile switch was not installed in every context");
    Require(first_input.find("W3VRHudEditorMoveX") == std::string::npos &&
        first_input.find("W3VRHudEditorDrag") == std::string::npos &&
        first_input.find("W3VRHudEditorExit") == std::string::npos &&
        first_input.find("IK_Tab=(Action=W3VRHudEditorProfile)") ==
            std::string::npos,
        "obsolete HUD editor bindings were retained");
    Require(first_input.find("IK_F12=(Action=UnrelatedAction)") !=
            std::string::npos &&
        first_input.find("CustomBinding=keep") != std::string::npos &&
        first_input.find("Value=preserve") != std::string::npos,
        "HUD setup changed unrelated input settings");
    Require(fs::exists(filelist.wstring() + L".w3vr.bak") &&
        fs::exists(input.wstring() + L".w3vr.bak"),
        "HUD setup did not preserve backups");

    const std::wstring manual_guide =
        w3vr::HudEditorManualSetupInstructions(paths);
    Require(manual_guide.find(filelist.wstring()) != std::wstring::npos &&
        manual_guide.find(input.wstring()) != std::wstring::npos &&
        manual_guide.find(L"modWitcher3VRHUDEditor.xml;") !=
            std::wstring::npos &&
        manual_guide.find(L"IK_Insert=(Action=W3VRHudEditorToggle)") !=
            std::wstring::npos &&
        manual_guide.find(L"IK_F7=(Action=W3VRHudEditorProfile)") !=
            std::wstring::npos &&
        manual_guide.find(L"IK_Tab=(Action=W3VRHudEditorProfile)") ==
            std::wstring::npos &&
        manual_guide.find(L"IK_Escape=(Action=W3VRHudEditorExit)") ==
            std::wstring::npos,
        "manual HUD recovery guide is incomplete or stale");

    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error),
        "idempotent HUD editor setup failed");
    Require(Read(filelist) == first_filelist && Read(input) == first_input,
        "repeated HUD editor setup changed already-correct files");

    const std::string utf16_le_expected = Utf16Bytes(
        u"audio.xml;\r\nBrothersInArms.xml;\r\n"
        u"modWitcher3VRHUDEditor.xml;\r\n");
    Write(filelist, Utf16Bytes(
        u"audio.xml;\r\nBrothersInArms.xml;\r\n"));
    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error),
        "UTF-16LE HUD editor registration failed");
    Require(Read(filelist) == utf16_le_expected,
        "UTF-16LE filelist encoding or newlines were not preserved");
    Require(CountOccurrences(Read(filelist),
        "modWitcher3VRHUDEditor.xml;") == 0,
        "HUD editor registration was appended as narrow text to UTF-16LE");
    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error) &&
        Read(filelist) == utf16_le_expected,
        "repeated UTF-16LE HUD editor setup was not idempotent");

    std::string mixed_encoding = Utf16Bytes(
        u"audio.xml;\r\nBrothersInArms.xml;");
    mixed_encoding += "\r\nmodWitcher3VRHUDEditor.xml;\r\n";
    Write(filelist, mixed_encoding);
    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error),
        "mixed UTF-16LE/ASCII HUD filelist repair failed");
    Require(Read(filelist) == utf16_le_expected,
        "mixed-encoding HUD filelist was not repaired exactly");
    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error) &&
        Read(filelist) == utf16_le_expected,
        "repaired UTF-16LE HUD filelist was not idempotent");

    const std::string utf16_be_expected = Utf16Bytes(
        u"audio.xml;\ninput.xml;\nmodWitcher3VRHUDEditor.xml;\n", false);
    Write(filelist, Utf16Bytes(u"audio.xml;\ninput.xml;\n", false));
    error.clear();
    Require(w3vr::EnsureHudEditorSetup(paths, error),
        "UTF-16BE HUD editor registration failed");
    Require(Read(filelist) == utf16_be_expected,
        "UTF-16BE filelist encoding or newlines were not preserved");
}

void TestDlssNearSquareResolutionCompatibility() {
    w3vr::LauncherState state;
    state.width = 2560;
    state.height = 2560;
    state.mode = w3vr::RenderMode::StereoDlssSequential;
    state.dlss_quality = 3;
    Require(w3vr::DlssNearSquareCompatibleWidth(state) == 2512,
        "stereo scaled DLSS square resolution was not adjusted by 48 pixels");

    state.width = 2880;
    state.height = 2880;
    state.mode = w3vr::RenderMode::MonoDlss;
    state.dlss_quality = 1;
    Require(w3vr::DlssNearSquareCompatibleWidth(state) == 2832,
        "mono scaled DLSS square resolution was not adjusted by 48 pixels");

    state.width = 2160;
    state.height = 2193;
    Require(w3vr::DlssNearSquareCompatibleWidth(state) == 2112,
        "portrait near-square DLSS resolution was not moved away from square");

    state.width = 2193;
    state.height = 2160;
    Require(w3vr::DlssNearSquareCompatibleWidth(state) == 2241,
        "landscape near-square DLSS resolution was not moved away from square");

    state.dlss_quality = 0;
    Require(!w3vr::DlssNearSquareCompatibleWidth(state).has_value(),
        "DLAA square resolution must remain unchanged");

    state.dlss_quality = 4;
    state.mode = w3vr::RenderMode::StereoTaau;
    Require(!w3vr::DlssNearSquareCompatibleWidth(state).has_value(),
        "non-DLSS square resolution must remain unchanged");

    state.mode = w3vr::RenderMode::StereoDlssSequential;
    state.width = 2832;
    state.height = 2880;
    Require(!w3vr::DlssNearSquareCompatibleWidth(state).has_value(),
        "already corrected DLSS resolution must not be adjusted twice");

    state.width = 2833;
    state.height = 2880;
    Require(w3vr::DlssNearSquareCompatibleWidth(state) == 2785,
        "47-pixel DLSS difference must still be adjusted");

    state.width = 2832;
    Require(!w3vr::DlssNearSquareCompatibleWidth(state).has_value(),
        "48-pixel DLSS difference must be accepted");
}

void TestDlssLabelsAndLegacyAuto(const w3vr::ConfigPaths& paths) {
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::MonoNone)) ==
        L"Mono - No AA / FXAA", "mono No AA / FXAA label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::StereoNone)) ==
        L"Stereo - No AA / FXAA", "stereo No AA / FXAA label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(
            w3vr::RenderMode::StereoDlssSequential)) ==
        L"Stereo - DLSS", "stereo DLSS label mismatch");
    Require(w3vr::SettingsForMode(w3vr::RenderMode::StereoNone).openxr_mode == 3 &&
        w3vr::SettingsForMode(w3vr::RenderMode::StereoTaau).openxr_mode == 3 &&
        w3vr::SettingsForMode(
            w3vr::RenderMode::StereoDlssSequential).openxr_mode == 3,
        "all stereo modes must use OpenXR Mode 3");
    Require(w3vr::SettingsForMode(w3vr::RenderMode::MonoNone).openxr_mode == 2 &&
        w3vr::SettingsForMode(w3vr::RenderMode::MonoTaau).openxr_mode == 2 &&
        w3vr::SettingsForMode(w3vr::RenderMode::MonoDlss).openxr_mode == 2,
        "all mono modes must use OpenXR Mode 2");

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
    vr->Set("engine", "first_person_snap_turn_degrees", "90");
    Write(paths.vr_ini, vr->Serialize());
    const auto partial = w3vr::LoadConfiguration(paths);
    Require(!partial.state.diagnostic_logging,
        "partial manual diagnostics must not display as a full diagnostic run");
    Require(partial.state.first_person_snap_turn_degrees == 45,
        "unsupported snap-turn angles must fall back to 45 degrees");

    vr->Remove("openxr", "cinema_full_vr");
    vr->Remove("openxr", "steady_icons");
    vr->Remove("openxr", "vertical_pitch_enabled");
    vr->Remove("engine", "first_person_combat_exit");
    vr->Remove("engine", "first_person_stationary_turn");
    vr->Remove("debug", "logging_enabled");
    vr->Remove("debug", "runtime_diagnostics");
    Write(paths.vr_ini, vr->Serialize());
    game->Remove("DLC", "DlcEnabled_movementinputfix");
    Write(paths.game_settings, game->Serialize());
    const auto missing_flags = w3vr::LoadConfiguration(paths);
    Require(missing_flags.state.cinema_full_vr,
        "missing automatic-cutscene flag must default to enabled");
    Require(!missing_flags.state.steady_icons,
        "missing steady-icons flag must default to disabled");
    Require(!missing_flags.state.vertical_pitch_enabled,
        "missing vertical-pitch flag must default to disabled");
    Require(missing_flags.state.first_person_combat_exit,
        "missing combat-switch flag must default to enabled");
    Require(!missing_flags.state.first_person_stationary_turn,
        "missing stationary-turn flag must default to disabled");
    Require(missing_flags.state.fast_movement_transitions,
        "missing faster-transitions DLC flag must default to enabled");
    Require(!missing_flags.state.diagnostic_logging,
        "missing diagnostic flags must default to disabled");
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
        "[meta]\r\n"
        "config_version=6\r\n"
        "[openxr]\r\n"
        "mode=3\r\n"
        "render_width=2688\r\n"
        "render_height=2784\r\n"
        "hud_horizontal_scale=1.000\r\n"
        "hud_vertical_scale=1.000\r\n"
        "cinema_5x4=1\r\n"
        "cinema_hud_scale=1.300\r\n"
        "manual_cinema_hud_scale=1.600\r\n"
        "cinema_full_vr=1\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "first_person_combat_exit=1\r\n"
        "first_person_stationary_turn=0\r\n"
        "[debug]\r\n"
        "logging_enabled=0\r\n"
        "runtime_diagnostics=0\r\n";
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "first-run INI creation failed");
    Require(created, "first-run INI was not reported as created");
    Require(Read(paths.vr_ini) == defaults,
        "first-run INI does not match embedded defaults");

    Write(paths.vr_ini,
        "; old alpha user file\r\n"
        "[openxr]\r\n"
        "mode=2\r\n"
        "render_width=3100\r\n"
        "render_height=3200\r\n"
        "cinema_5x4=0\r\n"
        "cinema_full_vr=1\r\n"
        "cinema_hud_stereo_shift_px=-36\r\n"
        "manual_cinema_hud_scale=1.000\r\n"
        "cinema_subtitle_stereo_shift_px=-88\r\n"
        "custom_user_value=keep\r\n"
        "[reverse]\r\n"
        "enabled=1\r\n"
        "[debug]\r\n"
        "logging_enabled=1\r\n");
    created = true;
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "existing INI migration failed");
    Require(!created, "migrated INI must not be reported as newly created");
    auto migrated = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated.has_value(), "migrated INI could not be read");
    Require(migrated->Get("meta", "config_version") == "6",
        "old INI was not versioned");
    Require(migrated->Get(
            "focus_projection", "shader_registry_enabled") == "0",
        "shader registry migration default missing");
    Require(migrated->Get("openxr", "mode") == "2" &&
        migrated->Get("openxr", "render_width") == "3100" &&
        migrated->Get("openxr", "render_height") == "3200",
        "migration changed mode or resolution");
    Require(migrated->Get("openxr", "cinema_5x4") == "1" &&
        migrated->Get("openxr", "cinema_render_stereo_strength") == "0.250" &&
        migrated->Get("openxr", "cinema_hud_stereo_shift_px") == "-72" &&
        migrated->Get("openxr", "manual_cinema_hud_scale") == "1.600" &&
        migrated->Get("openxr", "full_vr_hud_stereo_shift_px") == "-36" &&
        migrated->Get("openxr", "full_vr_hud_scale") == "1.000" &&
        migrated->Get("openxr", "cinema_hud_scale") == "1.300" &&
        migrated->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated->Get("openxr", "hud_vertical_scale") == "1.000" &&
        migrated->Get("engine", "first_person_stationary_turn") == "0",
        "migration did not apply the validated release defaults");
    Require(migrated->Get("openxr", "cinema_full_vr") == "1",
        "migration changed the existing Full VR choice");
    Require(migrated->Get("openxr", "custom_user_value") == "keep",
        "migration removed an unrelated user value");
    Require(!migrated->Get("openxr", "cinema_subtitle_stereo_shift_px").has_value(),
        "migration retained an obsolete subtitle key");
    Require(migrated->Get("reverse", "enabled") == "0" &&
        migrated->Get("debug", "logging_enabled") == "0",
        "migration did not restore release-safe flags");

    migrated->Set("openxr", "manual_cinema_hud_scale", "1.100");
    migrated->Set("openxr", "hud_horizontal_scale", "0.900");
    migrated->Set("openxr", "hud_vertical_scale", "0.950");
    migrated->Set("focus_projection", "shader_registry_enabled", "1");
    Write(paths.vr_ini, migrated->Serialize());
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "versioned INI check failed");
    auto versioned = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(versioned.has_value() &&
        versioned->Get("openxr", "manual_cinema_hud_scale") == "1.100" &&
        versioned->Get("openxr", "hud_horizontal_scale") == "0.900" &&
        versioned->Get("openxr", "hud_vertical_scale") == "0.950" &&
        versioned->Get(
            "focus_projection", "shader_registry_enabled") == "1",
        "current-version INI manual tuning was overwritten");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=2\r\n"
        "[openxr]\r\n"
        "cinema_hud_stereo_shift_px=-91\r\n"
        "cinema_hud_scale=1.100\r\n"
        "full_vr_hud_stereo_shift_px=-220\r\n"
        "full_vr_hud_scale=0.900\r\n"
        "hud_horizontal_scale=0.400\r\n"
        "hud_vertical_scale=0.700\r\n"
        "[engine]\r\n"
        "first_person_stationary_turn=0\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V2-to-V6 migration failed");
    auto migrated_v4 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v4.has_value() &&
        migrated_v4->Get("meta", "config_version") == "6" &&
        migrated_v4->Get("openxr", "cinema_hud_stereo_shift_px") == "-91" &&
        migrated_v4->Get("openxr", "cinema_hud_scale") == "1.100" &&
        migrated_v4->Get("openxr", "full_vr_hud_stereo_shift_px") == "-26" &&
        migrated_v4->Get("openxr", "full_vr_hud_scale") == "1.000" &&
        migrated_v4->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated_v4->Get("openxr", "hud_vertical_scale") == "1.000" &&
        migrated_v4->Get("engine", "first_person_stationary_turn") == "0" &&
        migrated_v4->Get(
            "focus_projection", "shader_registry_enabled") == "0",
        "V6 migration changed the Full VR offset or missed registry/HUD defaults");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=3\r\n"
        "[openxr]\r\n"
        "hud_horizontal_scale=0.620\r\n"
        "hud_vertical_scale=0.780\r\n"
        "custom_user_value=keep\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V3-to-V6 migration failed");
    auto migrated_from_v3 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_from_v3.has_value() &&
        migrated_from_v3->Get("meta", "config_version") == "6" &&
        migrated_from_v3->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated_from_v3->Get("openxr", "hud_vertical_scale") == "1.000" &&
        migrated_from_v3->Get("openxr", "custom_user_value") == "keep",
        "V3-to-V6 HUD migration damaged unrelated settings");
}

void TestVrBaselineAndRestore(const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    const std::string original = Read(paths.game_settings);
    const std::string profile =
        "[Localization]\r\n"
        "SpeechLanguage=EN\r\n"
        "TextLanguage=IT\r\n"
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
    Require(configured->Get("Localization", "SpeechLanguage") == "DE" &&
        configured->Get("Localization", "TextLanguage") == "DE",
        "target language settings were not preserved");

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
        TestDlssNearSquareResolutionCompatibility();
        TestProportionalCutsceneConvergence();
        TestReleaseDefaults();
        TestHudEditorSetup(temporary.path);
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
