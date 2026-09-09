#include "config.h"
#include "ofxr_launch_environment.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

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

std::vector<std::wstring> ParseEnvironmentBlock(
    const std::vector<wchar_t>& block) {
    std::vector<std::wstring> entries;
    for (size_t offset = 0; offset < block.size() && block[offset] != L'\0';) {
        const std::wstring entry(block.data() + offset);
        entries.push_back(entry);
        offset += entry.size() + 1;
    }
    return entries;
}

size_t CountEnvironmentVariable(
    const std::vector<std::wstring>& entries, std::wstring_view name) {
    const std::wstring prefix = std::wstring(name) + L"=";
    return static_cast<size_t>(std::count_if(
        entries.begin(), entries.end(), [&prefix](const std::wstring& entry) {
            return entry.size() >= prefix.size() &&
                _wcsnicmp(entry.c_str(), prefix.c_str(), prefix.size()) == 0;
        }));
}

bool HasEnvironmentEntry(
    const std::vector<std::wstring>& entries, std::wstring_view expected) {
    return std::find(entries.begin(), entries.end(), expected) != entries.end();
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
    return {root, root / "witcher3vr.ini", root / "optiscaler_bridge.ini",
        root / "ofxr_bridge.ini", root / "dx12user.settings",
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
        "world_detail_range=0.650\r\n"
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
        "raytracing_enabled=1\r\n"
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
        "critical_thread_core_isolation=1\r\n"
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
        "[Rendering/RT]\r\n"
        "EnableRT=true\r\n"
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
        state.resolution_auto = index % 2 == 0;
        state.width = 2496 + index;
        state.height = 2592 + index;
        state.dlss_quality = index % 5;
        state.integration_mode = w3vr::IntegrationMode::Optiscaler;
        state.frame_generation_backend =
            static_cast<w3vr::FrameGenerationBackend>(index %
                static_cast<int>(w3vr::FrameGenerationBackend::Count));
        state.hud_convergence_delta = 7;
        state.presentation_scale = 0.85f;
        state.world_detail_range = 0.65f;
        state.menu_scale = 0.75f;
        state.cinema_scale = 1.1f;
        state.cinema_height = -0.35f;
        state.cinema_aspect = static_cast<w3vr::CinemaAspect>(index %
            static_cast<int>(w3vr::CinemaAspect::Count));
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
        state.camera_follow_policy = static_cast<w3vr::CameraFollowPolicy>(
            index % 3);
        state.hide_static_hud_outside_combat = index % 2 == 0;
        state.fast_movement_transitions = index % 2 == 0;
        state.fullscreen_projection = true;
        state.diagnostic_logging = true;
        state.route_logging = true;
        state.performance_logging = true;
        state.renderdoc_enabled = true;

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
        Require(vr.Get("openxr", "resolution_auto") ==
            std::string(state.resolution_auto ? "1" : "0"),
            "wrong OpenXR AUTO resolution preference");
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
        Require(vr.Get("engine", "raytracing_enabled") == "1" &&
            game.Get("Rendering/RT", "EnableRT") == "true",
            "launcher must not own or overwrite Ray Tracing settings");
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
        Require(vr.Get("openxr", "presentation_scale") ==
            "0.850",
            "presentation scale must be preserved in every render mode");
        Require(vr.Get("openxr", "world_detail_range") == "0.650",
            "world-detail range must be preserved in every render mode");
        Require(vr.Get("openxr", "native_stereo") == "1",
            "ASYM must start enabled in every render mode");
        Require(vr.Get("openxr", "fullscreen_projection") == "1",
            "fullscreen projection must remain available in every render mode");
        Require(!vr.Get(
                "openxr", "alternate_presentation_resize").has_value(),
            "removed alternate presentation resize key was retained");
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
        Require(!vr.Get("engine", "critical_thread_core_isolation").has_value() &&
            !vr.Get("engine", "gamepad_select_recenter").has_value() &&
            !vr.Get("engine", "gamepad_select_first_person").has_value() &&
            !vr.Get("engine", "streamline_mvec_probe").has_value() &&
            !vr.Get("engine", "streamline_output_probe").has_value() &&
            !vr.Get("engine", "streamline_mvec_correction").has_value() &&
            !vr.Get("engine", "streamline_reconstruct_camera_motion").has_value() &&
            !vr.Get("engine", "streamline_force_camera_mvec").has_value(),
            "obsolete launcher and Streamline trial keys were not removed");
        Require(vr.Get("openxr", "cinema_scale") == "1.100",
            "cinema scale missing");
        Require(vr.Get("openxr", "cinema_height") == "-0.350",
            "cinema height missing");
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
            std::string(w3vr::CinemaAspectIniValue(state.cinema_aspect)) &&
            vr.Get("openxr", "cinema_5x4") ==
                std::string(expected_five_four ? "1" : "0"),
            "Cinema aspect and compatibility mirror mismatch");
        Require(vr.Get("meta", "config_version") == "19",
            "configuration version marker missing");
        Require(vr.Get("openxr", "resolution_auto") ==
            std::string(state.resolution_auto ? "1" : "0"),
            "OpenXR AUTO resolution preference missing");
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
        Require(game.Get("W3VRSettings", "CameraFollowPolicy") ==
                std::to_string(index % 3),
            "dynamic camera-follow policy missing");
        Require(game.Get("Gameplay", "AutoCameraCenter") ==
                std::string(index % 3 == 0 ? "true" : "false"),
            "native camera-follow bootstrap is inconsistent");
        Require(game.Get("W3VRSettings", "HideStaticHudOutsideCombat") ==
                std::string(index % 2 == 0 ? "true" : "false"),
            "dynamic static-HUD policy missing");
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
        Require(vr.Get("debug", "route_flight_recorder") == "1" &&
            vr.Get("debug", "pipeline_flight_recorder") == "1",
            "independent lightweight log settings were not saved");
        Require(vr.Get("renderdoc", "enabled") == "1",
            "RenderDoc launcher setting was not saved");
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

        w3vr::IniDocument optiscaler;
        Require(w3vr::BuildUpdatedOptiscalerDocument(
            paths, state, optiscaler, error),
            "OptiScaler sidecar document build failed");
        const bool expected_optiscaler = w3vr::ModeUsesDlss(state.mode);
        Require(optiscaler.Get("optiscaler", "enabled") ==
                std::string(expected_optiscaler ? "1" : "0"),
            "OptiScaler was not constrained to a DLSS render profile");
        w3vr::IniDocument ofxr;
        Require(w3vr::BuildUpdatedOfxrDocument(
                paths, state, ofxr, error),
            "OFXR Bridge sidecar document build failed");
        const std::array<std::string, 3> expected_ofxr{
            "off", "fidelityfx", "nvidia"};
        Require(ofxr.Get("ofxr", "backend") ==
                expected_ofxr[static_cast<size_t>(
                    state.frame_generation_backend)],
            "OFXR Bridge backend was not persisted");
        Write(paths.vr_ini, vr.Serialize());
        Write(paths.optiscaler_bridge_ini, optiscaler.Serialize());
        Write(paths.ofxr_bridge_ini, ofxr.Serialize());
        Write(paths.game_settings, game.Serialize());
        const auto loaded = w3vr::LoadConfiguration(paths);
        Require(loaded.warning.empty(), "saved mode should infer exactly");
        Require(loaded.state.mode == state.mode, "round-trip mode mismatch");
        Require(loaded.state.width == state.width && loaded.state.height == state.height,
            "round-trip resolution mismatch");
        Require(loaded.state.resolution_auto == state.resolution_auto,
            "round-trip OpenXR AUTO resolution mismatch");
        Require(loaded.state.fullscreen_projection,
            "round-trip fullscreen projection mismatch");
        Require(loaded.state.integration_mode ==
                (expected_optiscaler
                    ? w3vr::IntegrationMode::Optiscaler
                    : w3vr::IntegrationMode::Off),
            "round-trip OptiScaler sidecar mismatch");
        Require(loaded.state.frame_generation_backend ==
                state.frame_generation_backend,
            "round-trip OFXR Bridge backend mismatch");
        if (w3vr::ModeUsesDlss(state.mode)) {
            Require(loaded.state.dlss_quality == state.dlss_quality,
                "round-trip DLSS/DLAA selection mismatch");
        }
        Require(loaded.state.cinema_full_vr,
            "round-trip automatic full-VR cutscene mismatch");
        Require(loaded.state.cinema_aspect == state.cinema_aspect,
            "round-trip Cinema aspect mismatch");
        Require(loaded.state.cinema_height == state.cinema_height,
            "round-trip cinema height mismatch");
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
        Require(loaded.state.camera_follow_policy ==
                state.camera_follow_policy,
            "round-trip dynamic camera-follow policy mismatch");
        Require(loaded.state.hide_static_hud_outside_combat ==
                state.hide_static_hud_outside_combat,
            "round-trip dynamic static-HUD policy mismatch");
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
        Require(loaded.state.route_logging && loaded.state.performance_logging,
            "round-trip lightweight logging mismatch");
        Require(loaded.state.renderdoc_enabled,
            "round-trip RenderDoc setting mismatch");
    }

    // Always-on ASYM must remain independent from the fullscreen presenter.
    WriteBaseFixtures(paths);
    w3vr::LauncherState native_legacy;
    native_legacy.mode = w3vr::RenderMode::StereoNone;
    native_legacy.fullscreen_projection = false;
    w3vr::IniDocument vr;
    w3vr::IniDocument game;
    std::wstring error;
    Require(w3vr::BuildUpdatedDocuments(
        paths, native_legacy, vr, game, error),
        "independent presentation document build failed");
    Require(vr.Get("openxr", "native_stereo") == "1" &&
        vr.Get("openxr", "fullscreen_projection") == "0" &&
        !vr.Get("openxr", "alternate_presentation_resize").has_value(),
        "always-on ASYM still implies a presentation experiment");
}

void TestOptiscalerAndAlwaysAsym(
    const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    w3vr::LauncherState state;
    state.mode = w3vr::RenderMode::StereoTaau;
    state.presentation_scale = 0.85f;
    state.fullscreen_projection = false;
    state.integration_mode = w3vr::IntegrationMode::Off;
    w3vr::IniDocument vr;
    w3vr::IniDocument game;
    std::wstring error;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error),
        "launcher-owned renderer document build failed");
    Require(vr.Get("openxr", "native_stereo") == "1" &&
            vr.Get("openxr", "fullscreen_projection") == "0" &&
            !vr.Get("openxr", "alternate_presentation_resize").has_value(),
        "always-on ASYM or independent fullscreen projection is wrong");
    Require(vr.Get("engine", "raytracing_enabled") == "1" &&
            game.Get("Rendering/RT", "EnableRT") == "true",
        "removed launcher RTX path still overwrites RT settings");

    w3vr::IniDocument optiscaler;
    Require(w3vr::BuildUpdatedOptiscalerDocument(
            paths, state, optiscaler, error) &&
            optiscaler.Get("optiscaler", "enabled") == "0",
        "OptiScaler must default to disabled");
    state.integration_mode = w3vr::IntegrationMode::Optiscaler;
    Require(w3vr::BuildUpdatedOptiscalerDocument(
            paths, state, optiscaler, error) &&
            optiscaler.Get("optiscaler", "enabled") == "0",
        "OptiScaler must remain disabled outside a DLSS profile");
    Write(paths.vr_ini, vr.Serialize());
    Write(paths.game_settings, game.Serialize());
    Write(paths.optiscaler_bridge_ini, optiscaler.Serialize());
    Require(w3vr::LoadConfiguration(paths).state.integration_mode ==
            w3vr::IntegrationMode::Off,
        "non-DLSS profile exposed an enabled OptiScaler state");

    state.mode = w3vr::RenderMode::StereoDlssSequential;
    Require(w3vr::BuildUpdatedOptiscalerDocument(
            paths, state, optiscaler, error) &&
            optiscaler.Get("optiscaler", "enabled") == "1",
        "OptiScaler sidecar opt-in was not written for a DLSS profile");

    state.fullscreen_projection = true;
    Require(w3vr::BuildUpdatedDocuments(paths, state, vr, game, error) &&
            vr.Get("openxr", "fullscreen_projection") == "1" &&
            !vr.Get("openxr", "alternate_presentation_resize").has_value(),
        "advanced fullscreen projection did not remain independent");
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
    Require(defaults.mode == w3vr::RenderMode::AerAfwDlss &&
        defaults.dlss_quality == 3 &&
        defaults.width == 2688 && defaults.height == 2784,
        "AER DLSS Performance and resolution release defaults changed");
    Require(defaults.presentation_scale == 1.0f,
        "Presentation Size must default to 1.00");
    Require(defaults.world_detail_range == 1.0f,
        "World Detail Range must default to 100%");
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
    Require(defaults.integration_mode == w3vr::IntegrationMode::Off,
        "OptiScaler must default to disabled");
    Require(defaults.frame_generation_backend ==
            w3vr::FrameGenerationBackend::Off,
        "OFXR Bridge must default to Off");
    Require(!defaults.vertical_pitch_enabled,
        "vertical mouse/pad pitch must default to disabled");
    Require(!defaults.first_person_combat_exit,
        "automatic combat camera switch must default to disabled");
    Require(defaults.first_person_strafe,
        "first-person strafe must default to enabled");
    Require(defaults.first_person_anchor_smoothing,
        "first-person head-bobbing reduction must default to enabled");
    Require(defaults.camera_follow_policy ==
            w3vr::CameraFollowPolicy::HorseBoatOnly,
        "camera follow must default to the vehicle-only policy");
    Require(!defaults.hide_static_hud_outside_combat,
        "static-HUD hiding must remain opt-in");
    Require(defaults.fast_movement_transitions,
        "faster movement transitions must default to enabled");
    Require(!defaults.fullscreen_projection,
        "experimental fullscreen projection must default to disabled");
    Require(!defaults.diagnostic_logging,
        "diagnostic logging must default to disabled");
    Require(!defaults.route_logging,
        "lightweight route logging must default to disabled");
    Require(!defaults.performance_logging,
        "lightweight performance logging must default to disabled");
    Require(!defaults.renderdoc_enabled,
        "RenderDoc must default to disabled");
}

void TestVrProfileHighShadows() {
    std::wstring error;
    const auto profile_path =
        fs::path(__FILE__).parent_path() / "vr_dx12user.settings";
    const auto profile = w3vr::IniDocument::Load(profile_path, error);
    Require(profile.has_value(),
        "bundled Prepare Settings for VR profile could not be loaded");
    Require(profile->Get("PostProcess", "DLSSQuality") == "3" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale0") == "1" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale1") == "1" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale2") == "1.2" &&
        profile->Get("Rendering", "CascadeShadowDistanceScale3") == "1.2" &&
        profile->Get("Rendering", "CascadeShadowFadeTreshold") == "1" &&
        profile->Get("Rendering", "CascadeShadowmapSize") == "2048" &&
        profile->Get("Rendering", "CascadeShadowQuality") == "1" &&
        profile->Get("Rendering", "MaxTerrainShadowAtlasCount") == "4" &&
        profile->Get("Rendering/RT", "Shadows") == "true" &&
        profile->Get(
            "Rendering/SpeedTree", "FoliageShadowDistanceScale") == "16",
        "Prepare Settings for VR no longer carries Performance DLSS and High shadows");
}

void TestEmbeddedLauncherDefaults() {
    std::wstring error;
    const auto defaults_path =
        fs::path(__FILE__).parent_path() / "witcher3vr.default.ini";
    const auto defaults = w3vr::IniDocument::Load(defaults_path, error);
    Require(defaults.has_value(),
        "embedded launcher INI defaults could not be loaded");
    Require(defaults->Get("meta", "config_version") == "19" &&
        defaults->Get("openxr", "mode3_aer_presentation") == "1" &&
        defaults->Get("openxr", "resolution_auto") == "1" &&
        defaults->Get("openxr", "presentation_scale") == "1.000" &&
        !defaults->Get("openxr", "presentation_black_resize").has_value() &&
        defaults->Get("openxr", "world_detail_range") == "1.000" &&
        defaults->Get("openxr", "native_stereo") == "1" &&
        !defaults->Get(
            "openxr", "alternate_presentation_resize").has_value() &&
        defaults->Get("engine", "temporal_backend") == "dlss" &&
        defaults->Get("engine", "first_person_combat_exit") == "0" &&
        defaults->Get("engine", "raytracing_history_buffers") == "8" &&
        defaults->Get("renderdoc", "enabled") == "0" &&
        defaults->Get("renderdoc", "streamline_device_bridge") == "0" &&
        defaults->Get("launcher", "integration_mode") == "off" &&
        defaults->Get("debug", "pipeline_flight_recorder") == "0" &&
        defaults->Get("debug", "route_flight_recorder") == "0",
        "embedded launcher defaults do not match schema 19 release policy");
}

void TestIntegrationModeContract() {
    constexpr std::array<const wchar_t*, 6> expected_display{{
        L"Off", L"OptiScaler", L"ReShade", L"OptiScaler + ReShade",
        L"OptiScaler DLSS5", L"ReShade DLSS5 RenoDX"}};
    constexpr std::array<const char*, 6> expected_ini{{
        "off", "optiscaler", "reshade", "optiscaler_reshade",
        "optiscaler_dlss5", "reshade_dlss5"}};
    for (size_t index = 0; index < expected_display.size(); ++index) {
        const auto mode = static_cast<w3vr::IntegrationMode>(index);
        Require(std::wstring_view(w3vr::IntegrationModeDisplayName(mode)) ==
                expected_display[index],
            "integration display ordering changed");
        Require(std::string_view(w3vr::IntegrationModeIniValue(mode)) ==
                expected_ini[index] &&
            w3vr::ParseIntegrationMode(expected_ini[index]) == mode,
            "integration INI round-trip changed");
    }
    Require(!w3vr::IntegrationModeUsesOptiscaler(
                w3vr::IntegrationMode::Off) &&
            w3vr::IntegrationModeUsesOptiscaler(
                w3vr::IntegrationMode::Optiscaler) &&
            w3vr::IntegrationModeUsesOptiscaler(
                w3vr::IntegrationMode::OptiscalerReshade) &&
            w3vr::IntegrationModeUsesOptiscaler(
                w3vr::IntegrationMode::OptiscalerDlss5),
        "OptiScaler integration membership changed");
    Require(w3vr::IntegrationModeUsesReshade(
                w3vr::IntegrationMode::Reshade) &&
            w3vr::IntegrationModeUsesReshade(
                w3vr::IntegrationMode::OptiscalerReshade) &&
            w3vr::IntegrationModeUsesReshade(
                w3vr::IntegrationMode::ReshadeDlss5) &&
            !w3vr::IntegrationModeUsesReshade(
                w3vr::IntegrationMode::OptiscalerDlss5),
        "ReShade integration membership changed");
    Require(w3vr::IntegrationModeUsesDlss5(
                w3vr::IntegrationMode::OptiscalerDlss5) &&
            w3vr::IntegrationModeUsesDlss5(
                w3vr::IntegrationMode::ReshadeDlss5) &&
            !w3vr::IntegrationModeUsesDlss5(
                w3vr::IntegrationMode::OptiscalerReshade),
        "DLSS5 integration membership changed");
}

void TestHudEditorSetup(const fs::path& root) {
    const fs::path game = root / "hud-setup" / "game";
    const fs::path launcher_directory = game / "bin" / "x64_dx12";
    const fs::path documents = root / "hud-setup" / "Documents" /
        "The Witcher 3";
    const w3vr::ConfigPaths paths{
        launcher_directory,
        launcher_directory / "witcher3vr.ini",
        launcher_directory / "optiscaler_bridge.ini",
        launcher_directory / "ofxr_bridge.ini",
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
        "Mono DLSS square resolution was not adjusted by 48 pixels");

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
    Require(static_cast<int>(w3vr::RenderMode::MonoNone) == 0 &&
            static_cast<int>(w3vr::RenderMode::MonoTaau) == 1 &&
            static_cast<int>(w3vr::RenderMode::MonoDlss) == 2,
        "Mono modes must be the first three launcher entries");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::AerAfwTaau)) ==
        L"AER + AFW - TAAU", "AER + AFW TAAU label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::AerAfwDlss)) ==
        L"AER + AFW - DLSS", "AER + AFW DLSS label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::StereoNone)) ==
        L"Stereo - No AA / FXAA", "stereo No AA / FXAA label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(
            w3vr::RenderMode::StereoDlssSequential)) ==
        L"Stereo - DLSS", "stereo DLSS label mismatch");
    Require(std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::MonoNone)) ==
            L"Mono - No AA / FXAA" &&
        std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::MonoTaau)) ==
            L"Mono - TAAU" &&
        std::wstring(w3vr::ModeDisplayName(w3vr::RenderMode::MonoDlss)) ==
            L"Mono - DLSS",
        "Mono launcher labels mismatch");
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
    const auto mono_none =
        w3vr::SettingsForMode(w3vr::RenderMode::MonoNone);
    const auto mono_taau =
        w3vr::SettingsForMode(w3vr::RenderMode::MonoTaau);
    const auto mono_dlss =
        w3vr::SettingsForMode(w3vr::RenderMode::MonoDlss);
    Require(mono_none.openxr_mode == 1 && !mono_none.dual_render &&
            !mono_none.mode3_aer_presentation &&
            std::string(mono_none.temporal_backend) == "none" &&
            mono_none.aa_mode == 0 && !mono_none.allow_dlss &&
        mono_taau.openxr_mode == 1 && !mono_taau.dual_render &&
            !mono_taau.mode3_aer_presentation &&
            std::string(mono_taau.temporal_backend) == "taau" &&
            mono_taau.aa_mode == 3 && !mono_taau.allow_dlss &&
        mono_dlss.openxr_mode == 1 && !mono_dlss.dual_render &&
            !mono_dlss.mode3_aer_presentation &&
            std::string(mono_dlss.temporal_backend) == "dlss" &&
            mono_dlss.aa_mode == 6 && mono_dlss.allow_dlss,
        "Mono modes must use clean Mode 1 without dual-render or AER");
    Require(!w3vr::ModeUsesStereo(w3vr::RenderMode::MonoNone) &&
            !w3vr::ModeUsesStereo(w3vr::RenderMode::MonoTaau) &&
            !w3vr::ModeUsesStereo(w3vr::RenderMode::MonoDlss),
        "Mono must remain a single producer; ASYM is now a launcher invariant");

    WriteBaseFixtures(paths);
    std::wstring error;
    auto legacy_vr = w3vr::IniDocument::Load(paths.vr_ini, error);
    auto legacy_game = w3vr::IniDocument::Load(paths.game_settings, error);
    Require(legacy_vr.has_value() && legacy_game.has_value(),
        "legacy Mono fixture load failed");
    legacy_vr->Set("meta", "config_version", "14");
    legacy_vr->Set("openxr", "mode", "2");
    legacy_vr->Set("openxr", "mode3_aer_presentation", "1");
    legacy_vr->Set("engine", "temporal_backend", "none");
    legacy_vr->Set("engine", "dual_render_probe", "0");
    legacy_vr->Set("engine", "dual_render_start", "0");
    legacy_game->Set("PostProcess", "AAMode", "0");
    legacy_game->Set("Rendering", "AllowDLSS", "false");
    Write(paths.vr_ini, legacy_vr->Serialize());
    Write(paths.game_settings, legacy_game->Serialize());
    const auto migrated_mono = w3vr::LoadConfiguration(paths);
    Require(migrated_mono.warning.empty() &&
            migrated_mono.state.mode == w3vr::RenderMode::MonoNone,
        "V8-V14 Mode 2 must load as clean Mono No AA / FXAA");

    legacy_vr->Set("openxr", "mode", "1");
    legacy_vr->Set("openxr", "mode3_aer_presentation", "0");
    Write(paths.vr_ini, legacy_vr->Serialize());
    const auto migrated_aer = w3vr::LoadConfiguration(paths);
    Require(!migrated_aer.warning.empty() &&
            migrated_aer.state.mode == w3vr::RenderMode::StereoNone,
        "pre-V15 Mode 1 AER No AA must fall back visibly to Stereo No AA");

    legacy_vr->Set("meta", "config_version", "7");
    legacy_vr->Set("openxr", "mode", "2");
    legacy_vr->Set("openxr", "mode3_aer_presentation", "0");
    legacy_vr->Set("engine", "temporal_backend", "taau");
    legacy_game->Set("PostProcess", "AAMode", "3");
    Write(paths.vr_ini, legacy_vr->Serialize());
    Write(paths.game_settings, legacy_game->Serialize());
    const auto pre_v8_mode2 = w3vr::LoadConfiguration(paths);
    Require(pre_v8_mode2.warning.empty() &&
            pre_v8_mode2.state.mode == w3vr::RenderMode::AerAfwTaau,
        "pre-V8 Mode 2 must retain its historical AER meaning");

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
    game->Remove("W3VRSettings", "CameraFollowPolicy");
    game->Remove("W3VRSettings", "HideStaticHudOutsideCombat");
    game->Set("Gameplay", "AutoCameraCenter", "false");
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
    Require(missing_flags.state.camera_follow_policy ==
            w3vr::CameraFollowPolicy::HorseBoatOnly,
        "legacy disabled camera follow must migrate to vehicle-only");
    Require(!missing_flags.state.hide_static_hud_outside_combat,
        "missing static-HUD policy must default to disabled");
    Require(missing_flags.state.fast_movement_transitions,
        "missing faster-transitions DLC flag must default to enabled");
    Require(!missing_flags.state.diagnostic_logging,
        "missing diagnostic flags must default to disabled");

    game->Set("Gameplay", "AutoCameraCenter", "true");
    Write(paths.game_settings, game->Serialize());
    const auto legacy_camera_follow_on = w3vr::LoadConfiguration(paths);
    Require(legacy_camera_follow_on.state.camera_follow_policy ==
            w3vr::CameraFollowPolicy::AlwaysOn,
        "legacy enabled camera follow must migrate to Always On");
}

void TestFallbackAndAtomicSave(const w3vr::ConfigPaths& paths) {
    WriteBaseFixtures(paths);
    auto text = Read(paths.vr_ini);
    const auto cinema_missing = w3vr::LoadConfiguration(paths);
    Require(cinema_missing.state.cinema_scale == cinema_missing.state.menu_scale,
        "cinema fallback must match menu scale");
    Require(cinema_missing.state.cinema_height == -0.20f,
        "missing cinema height must keep the current default offset");

    w3vr::LauncherState state;
    state.mode = w3vr::RenderMode::AerAfwTaau;
    state.width = 3072;
    state.height = 3216;
    std::wstring error;
    Require(w3vr::SaveConfiguration(paths, state, error), "atomic save failed");
    Require(Read(paths.ofxr_bridge_ini).find("backend=off") != std::string::npos,
        "atomic save did not create the default OFXR Bridge sidecar");
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
    Write(paths.game_settings,
        "[PostProcess]\r\n"
        "AAMode=3\r\n"
        "DLSSQuality=1\r\n"
        "[Rendering]\r\n"
        "AllowDLSS=false\r\n");
    bool created{};
    std::wstring error;
    const std::string defaults =
        "[meta]\r\n"
        "config_version=19\r\n"
        "[openxr]\r\n"
        "mode=3\r\n"
        "mode3_aer_presentation=1\r\n"
        "resolution_auto=1\r\n"
        "render_width=2688\r\n"
        "render_height=2784\r\n"
        "world_detail_range=1.000\r\n"
        "native_stereo=1\r\n"
        "fullscreen_projection=0\r\n"
        "hud_horizontal_scale=1.000\r\n"
        "hud_vertical_scale=1.000\r\n"
        "cinema_aspect=5x4\r\n"
        "cinema_5x4=1\r\n"
        "cinema_hud_scale=1.300\r\n"
        "manual_cinema_hud_scale=1.600\r\n"
        "cinema_full_vr=1\r\n"
        "[engine]\r\n"
        "temporal_backend=dlss\r\n"
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
        "runtime_diagnostics=0\r\n"
        "pipeline_flight_recorder=0\r\n"
        "route_flight_recorder=0\r\n";
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "first-run INI creation failed");
    Require(created, "first-run INI was not reported as created");
    Require(Read(paths.vr_ini) == defaults,
        "first-run INI did not preserve the embedded release defaults");

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
    Require(migrated->Get("meta", "config_version") == "19",
        "old INI was not versioned");
    Require(migrated->Get("openxr", "resolution_auto") == "1",
        "old INI did not receive the OpenXR AUTO default");
    Require(migrated->Get("openxr", "world_detail_range") == "1.000",
        "old INI did not receive the full world-detail range default");
    Require(migrated->Get("openxr", "native_stereo") == "1" &&
        migrated->Get("openxr", "fullscreen_projection") == "0" &&
        !migrated->Get("openxr", "alternate_presentation_resize").has_value(),
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
        "pre-V8 Mode 2 did not retain its historical Mode-3 AER migration");
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

    // The live V1516 control was authored under schema 16. V17 must expose it
    // in the launcher without resetting the selected 50% range.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=16\r\n"
        "[openxr]\r\n"
        "world_detail_range=0.500\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V16-to-V17 range migration failed");
    const auto migrated_v16_range =
        w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v16_range.has_value() &&
            migrated_v16_range->Get("meta", "config_version") == "19" &&
            migrated_v16_range->Get(
                "openxr", "world_detail_range") == "0.500",
        "V17 migration reset the existing world-detail range");

    // Retire the old resize switch without changing the user's scale.
    Write(paths.vr_ini,
        "[meta]\r\nconfig_version=17\r\n"
        "[openxr]\r\npresentation_scale=0.750\r\n"
        "presentation_black_resize=1\r\n");
    Require(w3vr::EnsureVrConfiguration(paths, defaults, created, error),
        "retired resize migration failed");
    const auto migrated_presentation = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_presentation &&
            migrated_presentation->Get("openxr", "presentation_scale") == "0.750" &&
            !migrated_presentation->Get("openxr", "presentation_black_resize"),
        "retired resize survived or changed the presentation scale");

    // V15 assigns the clean Mono transport to Mode 1. Migrate each retired
    // pre-V15 numeric route according to V1357's effective runtime meaning,
    // regardless of stale AER/dual flags stored alongside it.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=14\r\n"
        "[openxr]\r\n"
        "mode=1\r\n"
        "mode3_aer_presentation=0\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=0\r\n"
        "dual_render_start=0\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V14 Mode-1 migration failed");
    const auto migrated_v14_mode1 =
        w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v14_mode1.has_value() &&
            migrated_v14_mode1->Get("meta", "config_version") == "19" &&
            migrated_v14_mode1->Get("openxr", "mode") == "3" &&
            migrated_v14_mode1->Get(
                "openxr", "mode3_aer_presentation") == "1" &&
            migrated_v14_mode1->Get(
                "engine", "temporal_backend") == "taau" &&
            migrated_v14_mode1->Get("engine", "dual_render_probe") == "1" &&
            migrated_v14_mode1->Get("engine", "dual_render_start") == "1",
        "V15 did not preserve pre-V15 Mode 1 as Mode-3 AER");
    const auto loaded_v14_mode1 = w3vr::LoadConfiguration(paths);
    Require(loaded_v14_mode1.warning.empty() &&
            loaded_v14_mode1.state.mode == w3vr::RenderMode::AerAfwTaau,
        "migrated pre-V15 Mode 1 did not load as AER + AFW TAAU");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=14\r\n"
        "[openxr]\r\n"
        "mode=2\r\n"
        "mode3_aer_presentation=1\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=1\r\n"
        "dual_render_start=1\r\n"
        "raytracing_enabled=1\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V14 Mode-2 migration failed");
    const auto migrated_v14_mode2 =
        w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v14_mode2.has_value() &&
            migrated_v14_mode2->Get("meta", "config_version") == "19" &&
            migrated_v14_mode2->Get("openxr", "mode") == "1" &&
            migrated_v14_mode2->Get(
                "openxr", "mode3_aer_presentation") == "0" &&
            migrated_v14_mode2->Get(
                "engine", "temporal_backend") == "taau" &&
            migrated_v14_mode2->Get("engine", "dual_render_probe") == "0" &&
            migrated_v14_mode2->Get("engine", "dual_render_start") == "0" &&
            migrated_v14_mode2->Get("engine", "raytracing_enabled") == "1" &&
            migrated_v14_mode2->Get("openxr", "native_stereo") == "1",
        "V15 did not convert pre-V15 Mode 2 to clean Mono while preserving the now-unmanaged RT value");
    const auto loaded_v14_mode2 = w3vr::LoadConfiguration(paths);
    Require(loaded_v14_mode2.warning.empty() &&
            loaded_v14_mode2.state.mode == w3vr::RenderMode::MonoTaau,
        "migrated pre-V15 Mode 2 did not load as Mono TAAU with ASYM");

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=14\r\n"
        "[openxr]\r\n"
        "mode=4\r\n"
        "mode3_aer_presentation=1\r\n"
        "[engine]\r\n"
        "temporal_backend=taau\r\n"
        "dual_render_probe=0\r\n"
        "dual_render_start=0\r\n");
    Require(w3vr::EnsureVrConfiguration(
        paths, defaults, created, error), "V14 Mode-4 migration failed");
    const auto migrated_v14_mode4 =
        w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_v14_mode4.has_value() &&
            migrated_v14_mode4->Get("meta", "config_version") == "19" &&
            migrated_v14_mode4->Get("openxr", "mode") == "3" &&
            migrated_v14_mode4->Get(
                "openxr", "mode3_aer_presentation") == "0" &&
            migrated_v14_mode4->Get("engine", "dual_render_probe") == "1" &&
            migrated_v14_mode4->Get("engine", "dual_render_start") == "1",
        "V15 did not preserve pre-V15 Mode 4 as strict Mode-3 Stereo");
    const auto loaded_v14_mode4 = w3vr::LoadConfiguration(paths);
    Require(loaded_v14_mode4.warning.empty() &&
            loaded_v14_mode4.state.mode == w3vr::RenderMode::StereoTaau,
        "migrated pre-V15 Mode 4 did not load as Stereo TAAU");

    // V12 repairs launcher-owned route fragments and removes trial keys that
    // have no runtime consumer, while preserving unrelated manual tuning.
    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=11\r\n"
        "[openxr]\r\n"
        "enabled=0\r\n"
        "mode=4\r\n"
        "mode3_aer_presentation=1\r\n"
        "resolution_auto=0\r\n"
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
        normalized_v12->Get("meta", "config_version") == "19" &&
        normalized_v12->Get("openxr", "enabled") == "1" &&
        normalized_v12->Get("openxr", "mode") == "3" &&
        normalized_v12->Get("openxr", "mode3_aer_presentation") == "0" &&
        normalized_v12->Get("openxr", "resolution_auto") == "0" &&
        normalized_v12->Get("openxr", "presentation_scale") == "1.000" &&
        normalized_v12->Get("openxr", "world_detail_range") == "1.000" &&
        normalized_v12->Get("openxr", "native_stereo") == "1" &&
        !normalized_v12->Get(
            "openxr", "alternate_presentation_resize").has_value() &&
        normalized_v12->Get("engine", "temporal_backend") == "none" &&
        normalized_v12->Get("engine", "raytracing_enabled") == "1" &&
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
        migrated_v4->Get("meta", "config_version") == "19" &&
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
        migrated_from_v3->Get("meta", "config_version") == "19" &&
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
        migrated_from_v6->Get("meta", "config_version") == "19" &&
        migrated_from_v6->Get("openxr", "native_stereo") == "1" &&
        migrated_from_v6->Get("openxr", "fullscreen_projection") == "0" &&
        !migrated_from_v6->Get(
            "openxr", "alternate_presentation_resize").has_value() &&
        migrated_from_v6->Get("openxr", "mode3_aer_presentation") == "0",
        "V6 combined flag was not split safely");

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
        migrated_from_v9->Get("meta", "config_version") == "19" &&
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
        migrated_from_v8->Get("meta", "config_version") == "19" &&
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

    Write(paths.vr_ini,
        "[meta]\r\n"
        "config_version=18\r\n"
        "[launcher]\r\n"
        "reshade_enabled=1\r\n"
        "dlss5_enabled=1\r\n");
    Require(w3vr::EnsureVrConfiguration(paths, defaults, created, error),
        "V18 integration selector migration failed");
    const auto migrated_integration =
        w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(migrated_integration.has_value() &&
            migrated_integration->Get("meta", "config_version") == "19" &&
            migrated_integration->Get(
                "launcher", "integration_mode") == "reshade_dlss5" &&
            !migrated_integration->Get(
                "launcher", "reshade_enabled").has_value() &&
            !migrated_integration->Get(
                "launcher", "dlss5_enabled").has_value(),
        "V18 independent integration booleans survived V19 migration");
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

void TestOfxrLaunchEnvironment(const fs::path& root) {
    const fs::path launcher = root / "ofxr-environment";
    std::vector<wchar_t> block;
    std::wstring error;

    Write(launcher / "ofxr" / "nested" / "retired.dll", "retired");
    Write(launcher / "OFXRBridgeTray.exe", "retired");
    Write(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.dll", "root-dll");
    Write(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.json", "root-json");
    Write(launcher / "ofxr_bridge.ini", "[ofxr]\nbackend=off\n");
    Require(w3vr::RemoveRetiredOfxrPayload(launcher, error),
        "retired OFXR payload cleanup failed");
    Require(!fs::exists(launcher / "ofxr") &&
            !fs::exists(launcher / "OFXRBridgeTray.exe"),
        "retired OFXR folder or tray executable survived cleanup");
    Require(fs::exists(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.dll") &&
            fs::exists(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.json") &&
            fs::exists(launcher / "ofxr_bridge.ini"),
        "retired OFXR cleanup removed a root-owned payload file");

    Require(!w3vr::BuildEnabledOfxrLaunchEnvironment(
            launcher, w3vr::FrameGenerationBackend::Off, block, error) &&
            block.empty() && !error.empty(),
        "OFXR environment builder must reject Off; legacy launch owns it");

    fs::remove(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.dll");
    fs::remove(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.json");
    Require(!w3vr::BuildEnabledOfxrLaunchEnvironment(
            launcher, w3vr::FrameGenerationBackend::Nvidia, block, error) &&
            error.find(L"missing") != std::wstring::npos,
        "OFXR selected backend must reject a missing bridge package");

    Write(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.dll", "fixture");
    Write(launcher / "XR_APILAYER_XRFrameBridge_diagnostic.json", "fixture");
    fs::remove(launcher / "ofxr_bridge.ini");
    Require(!w3vr::BuildEnabledOfxrLaunchEnvironment(
            launcher, w3vr::FrameGenerationBackend::Nvidia, block, error) &&
            error.find(L"ofxr_bridge.ini") != std::wstring::npos,
        "OFXR root layout must reject a missing root INI");
    Write(launcher / "ofxr_bridge.ini",
        "[ofxr]\nbackend=nvidia\n[diagnostics]\nlogging_enabled=1\n");
    Require(SetEnvironmentVariableW(
            L"XRFG_FLOW_BACKEND", L"parent-sentinel") != FALSE,
        "test could not seed the parent OFXR backend variable");

    Require(w3vr::BuildEnabledOfxrLaunchEnvironment(
            launcher, w3vr::FrameGenerationBackend::Nvidia, block, error),
        "NVIDIA OFXR child environment build failed");
    Require(block.size() >= 2 && block[block.size() - 1] == L'\0' &&
            block[block.size() - 2] == L'\0',
        "OFXR child environment must be double-NUL terminated");
    auto entries = ParseEnvironmentBlock(block);
    Require(HasEnvironmentEntry(entries,
            L"XR_API_LAYER_PATH=" + launcher.wstring()) &&
            HasEnvironmentEntry(entries,
                L"XR_ENABLE_API_LAYERS=XR_APILAYER_XRFrameBridge_diagnostic"),
        "NVIDIA OFXR child environment contains wrong values");
    Require(CountEnvironmentVariable(entries, L"XR_API_LAYER_PATH") == 1 &&
            CountEnvironmentVariable(entries, L"XR_ENABLE_API_LAYERS") == 1 &&
            CountEnvironmentVariable(entries, L"XRFG_FLOW_BACKEND") == 0,
        "OFXR child environment must use the root INI without a backend override");
    wchar_t parent_backend[32]{};
    Require(GetEnvironmentVariableW(
            L"XRFG_FLOW_BACKEND", parent_backend,
            static_cast<DWORD>(std::size(parent_backend))) > 0 &&
            std::wstring_view(parent_backend) == L"parent-sentinel",
        "OFXR environment builder mutated the launcher process");

    Require(w3vr::BuildEnabledOfxrLaunchEnvironment(
            launcher, w3vr::FrameGenerationBackend::FidelityFx, block, error),
        "FidelityFX OFXR child environment build failed");
    entries = ParseEnvironmentBlock(block);
    Require(HasEnvironmentEntry(entries,
            L"XR_API_LAYER_PATH=" + launcher.wstring()) &&
            CountEnvironmentVariable(entries, L"XRFG_FLOW_BACKEND") == 0,
        "FidelityFX OFXR launch must also defer backend selection to the root INI");
    SetEnvironmentVariableW(L"XRFG_FLOW_BACKEND", nullptr);
}

void TestOfxrBackendOnlySave() {
    TempDirectory temporary;
    const auto paths = MakePaths(temporary.path);
    WriteBaseFixtures(paths);
    std::wstring error;

    // Exercise the real Save Only / Save & Launch configuration path, including
    // a manual recorder change made after the launcher loaded its UI state.
    for (const auto backend : {w3vr::FrameGenerationBackend::Off,
            w3vr::FrameGenerationBackend::FidelityFx,
            w3vr::FrameGenerationBackend::Nvidia}) {
        for (const bool mod_logging : {false, true}) {
            for (const bool ofxr_logging : {false, true}) {
                for (const bool supplied_profile : {false, true}) {
                    Write(paths.ofxr_bridge_ini,
                        "[ofxr]\nbackend=off\n[diagnostics]\nlogging_enabled=" +
                        std::string(ofxr_logging ? "0" : "1") + "\n");
                    auto state = w3vr::LoadConfiguration(paths).state;
                    state.frame_generation_backend = backend;
                    state.diagnostic_logging = mod_logging;

                    // Deliberately mixed line endings, comments, unknown keys,
                    // a separate backend key and no trailing newline.
                    const std::string before =
                        "; OFXR configuration belongs to the DLL\r\n"
                        "[diagnostics]\n  logging_enabled=" +
                        std::string(ofxr_logging ? "1" : "0") +
                        "\nmax_file_mb=17\r\nflush_each_event=1\n\n"
                        "[ofxr]\r\n  backend =fidelityfx\r\n" +
                        (supplied_profile
                            ? "nvidia_preset=medium\nnvidia_input_scale=50\n"
                            : "nvidia_preset=slow\nnvidia_input_scale=100\n") +
                        "nvidia_bidirectional=1\r\n; keep this comment\n"
                        "future_option=keep\r\n[unrelated]\nbackend=untouched";
                    Write(paths.ofxr_bridge_ini, before);
                    std::string expected = before;
                    const char* backend_value =
                        backend == w3vr::FrameGenerationBackend::Off ? "off" :
                        backend == w3vr::FrameGenerationBackend::Nvidia
                            ? "nvidia" : "fidelityfx";
                    expected.replace(expected.find("backend =fidelityfx"),
                        std::string("backend =fidelityfx").size(),
                        "backend =" + std::string(backend_value));
                    Require(w3vr::SaveConfiguration(paths, state, error),
                        "OFXR backend-only save failed");
                    Require(Read(paths.ofxr_bridge_ini) == expected,
                        "OFXR save changed bytes outside the backend value");
                    Require(w3vr::SaveConfiguration(paths, state, error) &&
                            Read(paths.ofxr_bridge_ini) == expected,
                        "repeated save changed the independent OFXR settings");
                }
            }
        }
    }

    // Missing backend is inserted without recreating the user's configuration.
    Write(paths.ofxr_bridge_ini,
        "[ofxr]\r\nnvidia_preset=medium\r\nnvidia_input_scale=50\r\n"
        "[diagnostics]\r\nlogging_enabled=1\r\n");
    w3vr::LauncherState state;
    state.frame_generation_backend = w3vr::FrameGenerationBackend::Nvidia;
    w3vr::IniDocument ofxr;
    Require(w3vr::BuildUpdatedOfxrDocument(paths, state, ofxr, error) &&
            ofxr.Get("ofxr", "backend") == "nvidia" &&
            ofxr.Get("ofxr", "nvidia_preset") == "medium" &&
            ofxr.Get("ofxr", "nvidia_input_scale") == "50" &&
            ofxr.Get("diagnostics", "logging_enabled") == "1",
        "inserting the missing OFXR backend lost existing settings");

    // Fixed release settings are supplied once, not imposed during saves.
    const auto defaults = w3vr::IniDocument::Load(
        fs::path(__FILE__).parent_path().parent_path() / "ofxr_bridge.ini", error);
    Require(defaults && defaults->Get("ofxr", "nvidia_preset") == "medium" &&
            defaults->Get("ofxr", "nvidia_input_scale") == "50" &&
            defaults->Get("diagnostics", "logging_enabled") == "0",
        "supplied OFXR profile must remain Medium/50 with logging off");
}

void TestNonblockingVersion() {
    TempDirectory temporary;
    const auto paths = MakePaths(temporary.path);
    WriteBaseFixtures(paths);
    std::wstring error;
    const auto defaults = Read(fs::path(__FILE__).parent_path() /
        "witcher3vr.default.ini");
    Write(paths.vr_ini, defaults);
    auto ini = w3vr::IniDocument::Load(paths.vr_ini, error);
    Require(ini.has_value(), "version fixture failed");
    ini->Set("meta", "config_version", "99999");
    ini->Set("meta", "mod_version", "V99999");
    ini->Set("openxr", "presentation_scale", "0.750");
    ini->Set("openxr", "presentation_black_resize", "1");
    Write(paths.vr_ini, ini->Serialize());
    bool created{};
    Require(w3vr::EnsureVrConfiguration(paths, defaults, created, error),
        "configuration version became a blocking build requirement");
    const auto loaded = w3vr::LoadConfiguration(paths);
    w3vr::IniDocument vr, game;
    Require(w3vr::BuildUpdatedDocuments(paths, loaded.state, vr, game, error) &&
            vr.Get("openxr", "presentation_scale") == "0.750" &&
            !vr.Get("openxr", "presentation_black_resize") &&
            vr.Get("meta", "mod_version") == "V99999",
        "version-independent save failed or altered unrelated metadata");
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
        TestIntegrationModeContract();
        TestVrProfileHighShadows();
        TestHudEditorSetup(temporary.path);
        TestAllModes(paths);
        TestOptiscalerAndAlwaysAsym(paths);
        TestDlssLabelsAndLegacyAuto(paths);
        TestFallbackAndAtomicSave(paths);
        TestInconsistentWarning(paths);
        TestFirstRunConfiguration(temporary.path);
        TestVrBaselineAndRestore(paths);
        TestFailurePaths(temporary.path);
        TestOfxrLaunchEnvironment(temporary.path);
        TestOfxrBackendOnlySave();
        TestNonblockingVersion();
        std::cout << "All launcher configuration tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return 1;
    }
}
