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
        "raytracing_history_buffers=7\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "menu_state_probe=0\r\n"
        "view_probe=1\r\n"
        "frame_builder_probe=1\r\n"
        "close_camera_offset=0.750\r\n"
        "first_person_snap_turn=0\r\n"
        "first_person_snap_turn_degrees=45\r\n"
        "first_person_hmd_body_follow=0\r\n"
        "first_person_combat_exit=0\r\n"
        "first_person_stationary_turn=1\r\n"
        "first_person_strafe=0\r\n"
        "first_person_anchor_smoothing=0\r\n"
        "first_person_anchor_smoothing_seconds=0.125000\r\n"
        "streamline_ps93_learning_log=1\r\n"
        "gamepad_select_recenter=1\r\n"
        "gamepad_select_first_person=1\r\n"
        "streamline_mvec_probe=1\r\n"
        "streamline_output_probe=1\r\n"
        "streamline_mvec_correction=1\r\n"
        "streamline_reconstruct_camera_motion=1\r\n"
        "streamline_force_camera_mvec=1\r\n"
        "unrelated_engine=42\r\n"
        "\r\n"
        "[reverse]\r\n"
        "enabled=1\r\n"
        "scan_periodic=1\r\n"
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
    for (int index = 0;
        index < static_cast<int>(w3vr::RenderMode::Count); ++index) {
        WriteBaseFixtures(paths);
        w3vr::LauncherState state;
        state.mode = static_cast<w3vr::RenderMode>(index);
        state.width = 2496 + index;
        state.height = 2592 + index;
        state.dlss_quality = index % 5;
        state.ray_tracing = true;
        state.hud_convergence_delta = 7;
        state.presentation_scale = 0.85f;
        state.menu_scale = 0.75f;
        state.cinema_scale = 1.1f;
        state.cinema_aspect = index % 2 == 0
            ? w3vr::CinemaAspect::FiveFour
            : w3vr::CinemaAspect::FourThree;
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
        state.first_person_strafe = index % 2 != 0;
        state.first_person_anchor_smoothing = index % 2 == 0;
        state.fast_movement_transitions = index % 2 == 0;
        state.native_stereo = true;
        state.fullscreen_projection = true;
        state.alternate_presentation_resize = true;
        state.diagnostic_logging = true;

        w3vr::IniDocument vr;
        w3vr::IniDocument game;
        std::wstring error;
        Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error),
            "mode document build failed");
        const auto expected = w3vr::SettingsForMode(state.mode);
        Require(vr.Get("openxr", "mode") == std::to_string(expected.openxr_mode),
            "wrong OpenXR mode");
        Require(vr.Get("openxr", "mode3_aer_presentation") ==
            std::string(expected.mode3_aer_presentation ? "1" : "0"),
            "wrong reversible Mode-3 AER flag");
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
        const bool expected_ray_tracing =
            w3vr::ModeSupportsRayTracing(state.mode);
        Require(vr.Get("engine", "raytracing_enabled") ==
                std::string(expected_ray_tracing ? "1" : "0") &&
            game.Get("Rendering/RT", "EnableRT") ==
                std::string(expected_ray_tracing ? "true" : "false"),
            "Ray Tracing was not restricted to an AER + AFW temporal mode");
        Require(vr.Get("engine", "raytracing_history_buffers") == "7",
            "launcher overwrote the INI-only RTX history buffer count");
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
        const bool expected_native_stereo =
            w3vr::ModeUsesStereo(state.mode);
        Require(vr.Get("openxr", "presentation_scale") ==
            "0.850",
            "presentation scale must be preserved in every render mode");
        Require(vr.Get("openxr", "native_stereo") ==
            std::string(expected_native_stereo ? "1" : "0"),
            "native stereo must remain enabled in every stereo mode");
        Require(vr.Get("openxr", "fullscreen_projection") == "1",
            "fullscreen projection must remain available in every render mode");
        Require(vr.Get("openxr", "alternate_presentation_resize") == "0",
            "alternate presentation resize must be disabled by asymmetric projection");
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
        Require(!vr.Get("engine", "gamepad_select_recenter").has_value() &&
            !vr.Get("engine", "gamepad_select_first_person").has_value() &&
            !vr.Get("engine", "streamline_mvec_probe").has_value() &&
            !vr.Get("engine", "streamline_output_probe").has_value() &&
            !vr.Get("engine", "streamline_mvec_correction").has_value() &&
            !vr.Get("engine", "streamline_reconstruct_camera_motion").has_value() &&
            !vr.Get("engine", "streamline_force_camera_mvec").has_value(),
            "obsolete launcher and Streamline trial keys were not removed");
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
        const bool expected_five_four =
            state.cinema_aspect == w3vr::CinemaAspect::FiveFour;
        Require(vr.Get("openxr", "cinema_aspect") ==
            std::string(expected_five_four ? "5x4" : "4x3") &&
            vr.Get("openxr", "cinema_5x4") ==
                std::string(expected_five_four ? "1" : "0"),
            "Cinema aspect and compatibility mirror mismatch");
        Require(vr.Get("meta", "config_version") == "12",
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
        Require(!vr.Get("engine", "first_person_stationary_turn").has_value(),
            "obsolete stationary-turn control was not removed");
        Require(vr.Get("engine", "first_person_strafe") ==
            std::string(state.first_person_strafe ? "1" : "0"),
            "first-person strafe flag missing");
        Require(vr.Get("engine", "first_person_anchor_smoothing") ==
            std::string(state.first_person_anchor_smoothing ? "1" : "0"),
            "first-person head-bobbing reduction flag missing");
        Require(vr.Get("engine", "first_person_anchor_smoothing_seconds") ==
            "0.125000",
            "launcher overwrote the INI-only smoothing time");
        Require(vr.Get("debug", "logging_enabled") == "1",
            "diagnostic log writer flag missing");
        Require(vr.Get("debug", "runtime_diagnostics") == "1",
            "runtime diagnostics flag missing");
        Require(vr.Get("debug", "taau_drop_diagnostics") == "1",
            "per-eye TAAU diagnostics flag missing");
        Require(vr.Get("debug", "cinema_camera_diagnostics") == "1" &&
            vr.Get("debug", "first_person_state_diagnostics") == "1" &&
            vr.Get("debug", "first_person_aim_diagnostics") == "1" &&
            vr.Get("debug", "world_marker_diagnostics") == "1",
            "diagnostic checkbox does not own every runtime probe");
        Require(!vr.Get("debug", "cinema_subtitle_diagnostics").has_value(),
            "dead cinema subtitle diagnostic key was retained");
        Require(vr.Get("reverse", "enabled") == "0",
            "launcher must clear the obsolete reverse master workaround");
        Require(vr.Get("reverse", "scan_periodic") == "0",
            "launcher must clear obsolete reverse probe settings");
        Require(vr.Get("engine", "menu_state_probe") == "1" &&
            vr.Get("engine", "view_probe") == "0" &&
            vr.Get("engine", "frame_builder_probe") == "0",
            "launcher route normalization left stale engine probes enabled");
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
            w3vr::ModeUsesStereo(state.mode),
            "round-trip native stereo mode gate mismatch");
        Require(loaded.state.fullscreen_projection,
            "round-trip fullscreen projection mismatch");
        Require(!loaded.state.alternate_presentation_resize,
            "round-trip alternate presentation resize gate mismatch");
        Require(loaded.state.ray_tracing == expected_ray_tracing,
            "round-trip Ray Tracing mode gate mismatch");
        if (w3vr::ModeUsesDlss(state.mode)) {
            Require(loaded.state.dlss_quality == state.dlss_quality,
                "round-trip DLSS/DLAA selection mismatch");
        }
        Require(loaded.state.cinema_full_vr,
            "round-trip automatic full-VR cutscene mismatch");
        Require(loaded.state.cinema_aspect == state.cinema_aspect,
            "round-trip Cinema aspect mismatch");
        Require(loaded.state.steady_icons,
            "round-trip steady-icons latency mismatch");
        Require(loaded.state.first_person_gamepad_head_follow,
            "round-trip first-person gamepad head-follow mismatch");
        Require(loaded.state.first_person_snap_turn_degrees == 60,
            "round-trip first-person snap-turn angle mismatch");
        Require(loaded.state.first_person_combat_exit,
            "round-trip first-person combat-exit mismatch");
        Require(loaded.state.first_person_strafe == state.first_person_strafe,
            "round-trip first-person strafe mismatch");
        Require(loaded.state.first_person_anchor_smoothing ==
            state.first_person_anchor_smoothing,
            "round-trip first-person head-bobbing reduction mismatch");
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

    // Native Stereo must no longer imply the fullscreen presenter.
    WriteBaseFixtures(paths);
    w3vr::LauncherState native_legacy;
    native_legacy.mode = w3vr::RenderMode::StereoNone;
    native_legacy.native_stereo = true;
    native_legacy.fullscreen_projection = false;
    native_legacy.alternate_presentation_resize = false;
    w3vr::IniDocument vr;
    w3vr::IniDocument game;
    std::wstring error;
    Require(w3vr::BuildUpdatedDocuments(
        paths, native_legacy, vr, game, error),
        "independent presentation document build failed");
    Require(vr.Get("openxr", "native_stereo") == "1" &&
        vr.Get("openxr", "fullscreen_projection") == "0" &&
        vr.Get("openxr", "alternate_presentation_resize") == "0",
        "Native Stereo still implies a presentation experiment");
}

void TestControlledRayTracingAndAlternateResize(
    const w3vr::ConfigPaths& paths) {
    Require(w3vr::ModeSupportsRayTracing(w3vr::RenderMode::AerAfwDlss) &&
            w3vr::ModeSupportsRayTracing(w3vr::RenderMode::AerAfwTaau) &&
            !w3vr::ModeSupportsRayTracing(
                w3vr::RenderMode::StereoDlssSequential),
        "Ray Tracing support predicate does not match the AER + AFW temporal modes");
    Require(w3vr::AlternatePresentationResizeAvailable(
            w3vr::RenderMode::StereoTaau, false, 0.85f) &&
            !w3vr::AlternatePresentationResizeAvailable(
                w3vr::RenderMode::StereoTaau, false, 1.0f) &&
            !w3vr::AlternatePresentationResizeAvailable(
                w3vr::RenderMode::StereoTaau, true, 0.85f),
        "alternate resize availability does not follow size/asymmetric gates");

    WriteBaseFixtures(paths);
    w3vr::LauncherState state;
    state.mode = w3vr::RenderMode::StereoTaau;
    state.presentation_scale = 0.85f;
    state.native_stereo = false;
    state.fullscreen_projection = false;
    state.alternate_presentation_resize = true;
    state.ray_tracing = true;
    w3vr::IniDocument vr;
    w3vr::IniDocument game;
    std::wstring error;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error),
        "controlled renderer-option document build failed");
    Require(vr.Get("openxr", "fullscreen_projection") == "1" &&
            vr.Get("openxr", "alternate_presentation_resize") == "1",
        "alternate resize did not activate its required fullscreen presenter");
    Require(vr.Get("engine", "raytracing_enabled") == "0" &&
            game.Get("Rendering/RT", "EnableRT") == "false",
        "incompatible render mode did not force both Ray Tracing flags off");

    state.mode = w3vr::RenderMode::AerAfwTaau;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error) &&
            vr.Get("engine", "raytracing_enabled") == "1" &&
            game.Get("Rendering/RT", "EnableRT") == "true",
        "AER + AFW - TAAU did not enable both Ray Tracing flags");

    state.mode = w3vr::RenderMode::AerAfwDlss;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error) &&
            vr.Get("engine", "raytracing_enabled") == "1" &&
            game.Get("Rendering/RT", "EnableRT") == "true",
        "AER + AFW - DLSS did not enable both Ray Tracing flags");

    state.presentation_scale = 1.0f;
    state.fullscreen_projection = false;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error) &&
            vr.Get("openxr", "fullscreen_projection") == "0" &&
            vr.Get("openxr", "alternate_presentation_resize") == "0",
        "scale 1.00 did not sanitize the alternate presentation route");
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
    Require(defaults.mode == w3vr::RenderMode::StereoNone &&
        defaults.width == 2688 && defaults.height == 2784,
        "mode and resolution release defaults changed");
    Require(defaults.presentation_scale == 1.0f,
        "Presentation Size must default to 1.00");
    Require(defaults.full_vr_hud_scale == 1.0f &&
        w3vr::FullVrHudConvergenceShift(
            defaults.full_vr_hud_scale,
            defaults.full_vr_hud_convergence_offset) == -36,
        "Full VR cutscene HUD must default to size 1.0 at gameplay depth");
    Require(defaults.cinema_full_vr,
        "automatic Full VR cutscenes must default to enabled");
    Require(defaults.cinema_aspect == w3vr::CinemaAspect::FiveFour,
        "Cinema aspect must default to 5:4");
    Require(!defaults.steady_icons,
        "steady icons must default to disabled");
    Require(!defaults.ray_tracing,
        "Ray Tracing must default to disabled");
    Require(!defaults.vertical_pitch_enabled,
        "vertical mouse/pad pitch must default to disabled");
    Require(!defaults.first_person_combat_exit,
        "automatic combat camera switch must default to disabled");
    Require(defaults.first_person_strafe,
        "first-person strafe must default to enabled");
    Require(defaults.first_person_anchor_smoothing,
        "first-person head-bobbing reduction must default to enabled");
    Require(defaults.fast_movement_transitions,
        "faster movement transitions must default to enabled");
    Require(!defaults.native_stereo,
        "experimental native stereo must default to disabled");
    Require(!defaults.fullscreen_projection,
        "experimental fullscreen projection must default to disabled");
    Require(!defaults.alternate_presentation_resize,
        "alternate presentation resize must default to disabled");
    Require(!defaults.diagnostic_logging,
        "diagnostic logging must default to disabled");
}

void TestVrProfileExtremePlusShadows() {
    std::wstring error;
    const auto profile_path =
        fs::path(__FILE__).parent_path() / "vr_dx12user.settings";
    const auto profile = w3vr::IniDocument::Load(profile_path, error);
    Require(profile.has_value(),
        "bundled Prepare Settings for VR profile could not be loaded");
    Require(profile->Get("Rendering", "CascadeShadowDistanceScale0") == "1.8" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale1") == "1.5" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale2") == "1.5" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale3") == "1.5" &&
        profile->Get("Rendering", "CascadeShadowFadeTreshold") == "1" &&
        profile->Get("Rendering", "CascadeShadowmapSize") == "4096" &&
        profile->Get("Rendering", "CascadeShadowQuality") == "1" &&
        profile->Get("Rendering", "MaxTerrainShadowAtlasCount") == "4" &&
        profile->Get("Rendering/RT", "Shadows") == "true" &&
        profile->Get(
            "Rendering/SpeedTree", "FoliageShadowDistanceScale") == "16",
        "Prepare Settings for VR no longer carries the Extreme+ shadow profile");
}

void TestEmbeddedLauncherDefaults() {
    std::wstring error;
    const auto defaults_path =
        fs::path(__FILE__).parent_path() / "witcher3vr.default.ini";
    const auto defaults = w3vr::IniDocument::Load(defaults_path, error);
    Require(defaults.has_value(),
        "embedded launcher INI defaults could not be loaded");
    Require(defaults->Get("meta", "config_version") == "12" &&
        defaults->Get("openxr", "presentation_scale") == "1.000" &&
        defaults->Get("engine", "first_person_combat_exit") == "0" &&
        defaults->Get("engine", "raytracing_history_buffers") == "8",
        "embedded launcher defaults do not match schema 12 release policy");
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
    state.mode = w3vr::RenderMode::AerAfwDlss;
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
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::AerAfwTaau)) ==
        L"AER + AFW - TAAU", "AER + AFW TAAU label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::AerAfwDlss)) ==
        L"AER + AFW - DLSS", "AER + AFW DLSS label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::StereoNone)) ==
        L"Stereo - No AA / FXAA", "stereo No AA / FXAA label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(
            w3vr::RenderMode::StereoDlssSequential)) ==
        L"Stereo - DLSS", "stereo DLSS label mismatch");
    Require(w3vr::SettingsForMode(w3vr::RenderMode::StereoNone).openxr_mode == 3 &&
        !w3vr::SettingsForMode(
            w3vr::RenderMode::StereoNone).mode3_aer_presentation &&
        w3vr::SettingsForMode(w3vr::RenderMode::StereoTaau).openxr_mode == 3 &&
        !w3vr::SettingsForMode(
            w3vr::RenderMode::StereoTaau).mode3_aer_presentation &&
        w3vr::SettingsForMode(
            w3vr::RenderMode::StereoDlssSequential).openxr_mode == 3 &&
        !w3vr::SettingsForMode(
            w3vr::RenderMode::StereoDlssSequential).mode3_aer_presentation,
        "all Stereo modes must use strict OpenXR Mode 3");
    const auto aer_taau =
        w3vr::SettingsForMode(w3vr::RenderMode::AerAfwTaau);
    const auto aer_dlss =
        w3vr::SettingsForMode(w3vr::RenderMode::AerAfwDlss);
    Require(aer_taau.openxr_mode == 3 && aer_taau.dual_render &&
            aer_taau.mode3_aer_presentation &&
            std::string(aer_taau.temporal_backend) == "taau" &&
            aer_taau.aa_mode == 3 && !aer_taau.allow_dlss,
        "AER TAAU must use Mode 3 with per-eye publication");
    Require(aer_dlss.openxr_mode == 3 && aer_dlss.dual_render &&
            aer_dlss.mode3_aer_presentation &&
            std::string(aer_dlss.temporal_backend) == "dlss" &&
            aer_dlss.aa_mode == 6 && aer_dlss.allow_dlss,
        "AER DLSS must use Mode 3 with per-eye publication");

    WriteBaseFixtures(paths);
    std::wstring error;
    auto legacy_vr = w3vr::IniDocument::Load(paths.vr_ini, error);
    auto legacy_game = w3vr::IniDocument::Load(paths.game_settings, error);
    Require(legacy_vr.has_value() && legacy_game.has_value(),
        "legacy Mono fixture load failed");
    legacy_vr->Set("openxr", "mode", "2");
    legacy_vr->Set("engine", "temporal_backend", "none");
    legacy_vr->Set("engine", "dual_render_probe", "0");
    legacy_vr->Set("engine", "dual_render_start", "0");
    legacy_game->Set("PostProcess", "AAMode", "0");
    legacy_game->Set("Rendering", "AllowDLSS", "false");
    Write(paths.vr_ini, legacy_vr->Serialize());
    Write(paths.game_settings, legacy_game->Serialize());
    const auto migrated_mono = w3vr::LoadConfiguration(paths);
    Require(!migrated_mono.warning.empty() &&
            migrated_mono.state.mode == w3vr::RenderMode::StereoNone,
        "retired AER No AA must fall back visibly to Stereo No AA / FXAA");

    WriteBaseFixtures(paths);
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
    vr->Remove("engine", "first_person_strafe");
    vr->Remove("engine", "first_person_anchor_smoothing");
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
    Require(!missing_flags.state.first_person_combat_exit,
        "missing combat-switch flag must default to disabled");
    Require(missing_flags.state.first_person_strafe,
        "missing first-person strafe flag must default to enabled");
    Require(missing_flags.state.first_person_anchor_smoothing,
        "missing head-bobbing reduction flag must default to enabled");
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
    state.mode = w3vr::RenderMode::AerAfwTaau;
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
        "config_version=12\r\n"
        "[openxr]\r\n"
        "mode=3\r\n"
        "mode3_aer_presentation=0\r\n"
        "render_width=2688\r\n"
        "render_height=2784\r\n"
        "native_stereo=0\r\n"
        "fullscreen_projection=0\r\n"
        "alternate_presentation_resize=0\r\n"
        "hud_horizontal_scale=1.000\r\n"
        "hud_vertical_scale=1.000\r\n"
        "cinema_aspect=5x4\r\n"
        "cinema_5x4=1\r\n"
        "cinema_hud_scale=1.300\r\n"
        "manual_cinema_hud_scale=1.600\r\n"
        "cinema_full_vr=1\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "raytracing_enabled=0\r\n"
        "raytracing_history_buffers=8\r\n"
        "first_person_combat_exit=0\r\n"
        "first_person_strafe=1\r\n"
        "first_person_anchor_smoothing=1\r\n"
        "first_person_anchor_smoothing_seconds=0.200000\r\n"
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
    Require(migrated->Get("meta", "config_version") == "12",
        "old INI was not versioned");
    Require(migrated->Get("openxr", "native_stereo") == "0" &&
        migrated->Get("openxr", "fullscreen_projection") == "0" &&
        migrated->Get("openxr", "alternate_presentation_resize") == "0",
        "old INI did not receive safe independent presentation defaults");
    Require(migrated->Get(
            "focus_projection", "shader_registry_enabled") == "0",
        "shader registry migration default missing");
    Require(migrated->Get("openxr", "mode") == "3" &&
        migrated->Get("openxr", "mode3_aer_presentation") == "1" &&
        migrated->Get("engine", "dual_render_probe") == "1" &&
        migrated->Get("engine", "dual_render_start") == "1" &&
        migrated->Get("openxr", "render_width") == "3100" &&
        migrated->Get("openxr", "render_height") == "3200",
        "migration did not convert legacy Mono to Mode-3 AER cleanly");
    Require(migrated->Get("openxr", "cinema_aspect") == "5x4" &&
        migrated->Get("openxr", "cinema_5x4") == "1" &&
        migrated->Get("openxr", "cinema_render_stereo_strength") == "0.250" &&
        migrated->Get("openxr", "cinema_hud_stereo_shift_px") == "-72" &&
        migrated->Get("openxr", "manual_cinema_hud_scale") == "1.600" &&
        migrated->Get("openxr", "full_vr_hud_stereo_shift_px") == "-36" &&
        migrated->Get("openxr", "full_vr_hud_scale") == "1.000" &&
        migrated->Get("openxr", "cinema_hud_scale") == "1.300" &&
        migrated->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated->Get("openxr", "hud_vertical_scale") == "1.000" &&
        !migrated->Get("engine", "first_person_stationary_turn").has_value() &&
        migrated->Get("engine", "first_person_strafe") == "1" &&
        migrated->Get("engine", "first_person_anchor_smoothing") == "1" &&
        migrated->Get("engine", "first_person_anchor_smoothing_seconds") ==
            "0.200000",
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
    migrated->Set("engine", "first_person_strafe", "0");
    migrated->Set("engine", "first_person_anchor_smoothing", "0");
    migrated->Set("engine", "first_person_anchor_smoothing_seconds", "0.125000");
    migrated->Remove("engine", "raytracing_history_buffers");
    migrated->Remove("debug", "runtime_diagnostics");
    Write(paths.vr_ini, migrated->Serialize());
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "versioned INI check failed");
    auto versioned = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(versioned.has_value() &&
        versioned->Get("openxr", "manual_cinema_hud_scale") == "1.100" &&
        versioned->Get("openxr", "hud_horizontal_scale") == "0.900" &&
        versioned->Get("openxr", "hud_vertical_scale") == "0.950" &&
        versioned->Get(
            "focus_projection", "shader_registry_enabled") == "1" &&
        versioned->Get("engine", "first_person_strafe") == "0" &&
        versioned->Get("engine", "first_person_anchor_smoothing") == "0" &&
        versioned->Get("engine", "first_person_anchor_smoothing_seconds") ==
            "0.125000" &&
        versioned->Get("engine", "raytracing_history_buffers") == "8" &&
        versioned->Get("debug", "runtime_diagnostics") == "0",
        "current-version INI defaults were not materialized safely");

    // V12 repairs launcher-owned route fragments and removes trial keys that
    // have no runtime consumer, while preserving unrelated manual tuning.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=11\r\n"
        "[openxr]\r\n"
        "enabled=0\r\n"
        "mode=4\r\n"
        "mode3_aer_presentation=1\r\n"
        "presentation_scale=1.000\r\n"
        "native_stereo=1\r\n"
        "fullscreen_projection=0\r\n"
        "alternate_presentation_resize=1\r\n"
        "custom_user_value=keep\r\n"
        "[engine]\r\n"
        "temporal_backend=none\r\n"
        "raytracing_enabled=1\r\n"
        "dual_render_probe=0\r\n"
        "dual_render_start=0\r\n"
        "menu_state_probe=0\r\n"
        "view_probe=1\r\n"
        "frame_builder_probe=1\r\n"
        "gamepad_select_recenter=1\r\n"
        "streamline_mvec_probe=1\r\n"
        "streamline_mvec_correction=1\r\n"
        "[reverse]\r\n"
        "enabled=1\r\n"
        "scan_periodic=1\r\n"
        "[debug]\r\n"
        "logging_enabled=1\r\n"
        "runtime_diagnostics=0\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V11-to-V12 normalization failed");
    const auto normalized_v12 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(normalized_v12.has_value() &&
        normalized_v12->Get("meta", "config_version") == "12" &&
        normalized_v12->Get("openxr", "enabled") == "1" &&
        normalized_v12->Get("openxr", "mode") == "3" &&
        normalized_v12->Get("openxr", "mode3_aer_presentation") == "0" &&
        normalized_v12->Get("openxr", "presentation_scale") == "1.000" &&
        normalized_v12->Get("openxr", "native_stereo") == "1" &&
        normalized_v12->Get("openxr", "alternate_presentation_resize") == "0" &&
        normalized_v12->Get("engine", "temporal_backend") == "none" &&
        normalized_v12->Get("engine", "raytracing_enabled") == "0" &&
        normalized_v12->Get("engine", "dual_render_probe") == "1" &&
        normalized_v12->Get("engine", "dual_render_start") == "1" &&
        normalized_v12->Get("engine", "menu_state_probe") == "1" &&
        normalized_v12->Get("engine", "view_probe") == "0" &&
        normalized_v12->Get("engine", "frame_builder_probe") == "0" &&
        normalized_v12->Get("reverse", "enabled") == "0" &&
        normalized_v12->Get("reverse", "scan_periodic") == "0" &&
        normalized_v12->Get("debug", "logging_enabled") == "0" &&
        normalized_v12->Get("debug", "runtime_diagnostics") == "0" &&
        !normalized_v12->Get("engine", "gamepad_select_recenter").has_value() &&
        !normalized_v12->Get("engine", "streamline_mvec_probe").has_value() &&
        !normalized_v12->Get("engine", "streamline_mvec_correction").has_value() &&
        normalized_v12->Get("openxr", "custom_user_value") == "keep",
        "V12 normalization did not repair stale launcher-owned settings safely");

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
        paths, defaults, created, error), "V2-to-V11 migration failed");
    auto migrated_v4 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v4.has_value() &&
        migrated_v4->Get("meta", "config_version") == "12" &&
        migrated_v4->Get("openxr", "cinema_hud_stereo_shift_px") == "-91" &&
        migrated_v4->Get("openxr", "cinema_hud_scale") == "1.100" &&
        migrated_v4->Get("openxr", "full_vr_hud_stereo_shift_px") == "-26" &&
        migrated_v4->Get("openxr", "full_vr_hud_scale") == "1.000" &&
        migrated_v4->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated_v4->Get("openxr", "hud_vertical_scale") == "1.000" &&
        !migrated_v4->Get("engine", "first_person_stationary_turn").has_value() &&
        migrated_v4->Get("engine", "first_person_strafe") == "1" &&
        migrated_v4->Get("engine", "first_person_anchor_smoothing") == "1" &&
        migrated_v4->Get("engine", "first_person_anchor_smoothing_seconds") ==
            "0.200000" &&
        migrated_v4->Get(
            "focus_projection", "shader_registry_enabled") == "0",
        "V11 migration changed the Full VR offset or missed release defaults");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=3\r\n"
        "[openxr]\r\n"
        "hud_horizontal_scale=0.620\r\n"
        "hud_vertical_scale=0.780\r\n"
        "custom_user_value=keep\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V3-to-V11 migration failed");
    auto migrated_from_v3 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_from_v3.has_value() &&
        migrated_from_v3->Get("meta", "config_version") == "12" &&
        migrated_from_v3->Get("openxr", "hud_horizontal_scale") == "1.000" &&
        migrated_from_v3->Get("openxr", "hud_vertical_scale") == "1.000" &&
        migrated_from_v3->Get("openxr", "custom_user_value") == "keep",
        "V3-to-V11 HUD migration damaged unrelated settings");

    // V6 used fullscreen_projection as Native Stereo. Preserve that choice
    // under the dedicated key without opting the user into a new presenter.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=6\r\n"
        "[openxr]\r\n"
        "fullscreen_projection=1\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V6-to-V11 migration failed");
    auto migrated_from_v6 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_from_v6.has_value() &&
        migrated_from_v6->Get("meta", "config_version") == "12" &&
        migrated_from_v6->Get("openxr", "native_stereo") == "1" &&
        migrated_from_v6->Get("openxr", "fullscreen_projection") == "0" &&
        migrated_from_v6->Get("openxr", "alternate_presentation_resize") == "0" &&
        migrated_from_v6->Get("openxr", "mode3_aer_presentation") == "0",
        "V6 combined flag was not split safely");

    // V11 makes the launcher the sole owner of Ray Tracing compatibility.
    // An incompatible route must be disabled during migration, while the
    // already supported AER + AFW temporal opt-in remains selected.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=10\r\n"
        "[openxr]\r\n"
        "mode3_aer_presentation=0\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "raytracing_enabled=1\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V10-to-V11 RT migration failed");
    auto migrated_rt_incompatible = w3vr::IniDocument::Load(
        paths.vr_ini, error);
    Require(migrated_rt_incompatible.has_value() &&
            migrated_rt_incompatible->Get("meta", "config_version") == "12" &&
            migrated_rt_incompatible->Get(
                "engine", "raytracing_enabled") == "0",
        "V11 migration retained Ray Tracing on an incompatible mode");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=10\r\n"
        "[openxr]\r\n"
        "mode3_aer_presentation=1\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "raytracing_enabled=1\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "supported V10-to-V11 RT migration failed");
    auto migrated_rt_supported = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_rt_supported.has_value() &&
            migrated_rt_supported->Get("meta", "config_version") == "12" &&
            migrated_rt_supported->Get(
                "engine", "raytracing_enabled") == "1",
        "V11 migration removed the supported AER + AFW / TAAU RT opt-in");

    // V9 is the last V1138 launcher schema. Remove its retired stationary
    // switch while retaining every explicit V9531 First Person preference.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=9\r\n"
        "[engine]\r\n"
        "first_person_stationary_turn=0\r\n"
        "first_person_strafe=0\r\n"
        "first_person_anchor_smoothing=0\r\n"
        "first_person_anchor_smoothing_seconds=0.125000\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V9-to-V11 migration failed");
    auto migrated_from_v9 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_from_v9.has_value() &&
        migrated_from_v9->Get("meta", "config_version") == "12" &&
        !migrated_from_v9->Get(
            "engine", "first_person_stationary_turn").has_value() &&
        migrated_from_v9->Get("engine", "first_person_strafe") == "0" &&
        migrated_from_v9->Get("engine", "first_person_anchor_smoothing") ==
            "0" &&
        migrated_from_v9->Get(
            "engine", "first_person_anchor_smoothing_seconds") == "0.125000",
        "V9-to-V11 migration changed explicit First Person preferences");

    // V8 exposed only cinema_5x4. Preserve an explicit off value as the new
    // 4:3 selection rather than silently returning it to the 5:4 default.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=8\r\n"
        "[openxr]\r\n"
        "cinema_5x4=0\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V8-to-V11 migration failed");
    auto migrated_from_v8 = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_from_v8.has_value() &&
        migrated_from_v8->Get("meta", "config_version") == "12" &&
        migrated_from_v8->Get("openxr", "cinema_aspect") == "4x3" &&
        migrated_from_v8->Get("openxr", "cinema_5x4") == "0",
        "V8 Cinema framing choice was not migrated to 4:3");
    Write(paths.game_settings,
        "[PostProcess]\r\n"
        "AAMode=0\r\n"
        "[Rendering]\r\n"
        "AllowDLSS=false\r\n");
    const auto loaded_four_three = w3vr::LoadConfiguration(paths);
    Require(loaded_four_three.state.cinema_aspect ==
            w3vr::CinemaAspect::FourThree,
        "migrated 4:3 Cinema aspect was not loaded");
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
        TestEmbeddedLauncherDefaults();
        TestVrProfileExtremePlusShadows();
        TestHudEditorSetup(temporary.path);
        TestAllModes(paths);
        TestControlledRayTracingAndAlternateResize(paths);
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
