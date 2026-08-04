#include "config.h"
#include "resources.h"

#include <Windows.h>
#include <CommCtrl.h>
#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using w3vr::LauncherState;
using w3vr::RenderMode;

constexpr wchar_t kWindowClass[] = L"Witcher3VRLauncherWindow";
constexpr int kClientWidth = 720;
constexpr int kClientHeight = 1020;

enum ControlId {
    IdMode = 100,
    IdResolution,
    IdWidth,
    IdHeight,
    IdDlssQuality,
    IdConvergence,
    IdConvergenceValue,
    IdPresentationScale,
    IdPresentationScaleValue,
    IdHudHorizontalScale,
    IdHudHorizontalScaleValue,
    IdHudVerticalScale,
    IdHudVerticalScaleValue,
    IdMenuScale,
    IdMenuScaleValue,
    IdCinemaScale,
    IdCinemaScaleValue,
    IdNearView,
    IdNearViewValue,
    IdVerticalPitch,
    IdFirstPersonGamepadHeadFollow,
    IdFirstPersonSnapTurnDegrees,
    IdFirstPersonCombatExit,
    IdCinema5x4,
    IdSteadyIcons,
    IdDiagnosticLogging,
    IdStatus,
    IdConfigureVr,
    IdRestoreOriginal,
    IdRestoreDefaults,
    IdSave,
    IdSaveLaunch,
};

struct App {
    HWND window{};
    HFONT font{};
    w3vr::ConfigPaths paths;
    LauncherState loaded;
};

App g_app;

HWND Item(int id) {
    return GetDlgItem(g_app.window, id);
}

void ApplyFont(HWND control) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.font), TRUE);
}

HWND AddControl(const wchar_t* class_name, const wchar_t* text, DWORD style,
    int x, int y, int width, int height, int id = 0, DWORD ex_style = 0) {
    HWND control = CreateWindowExW(ex_style, class_name, text,
        WS_CHILD | WS_VISIBLE | style, x, y, width, height, g_app.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    ApplyFont(control);
    return control;
}

HWND AddLabel(const wchar_t* text, int x, int y, int width, int height,
    int id = 0, DWORD style = SS_LEFT) {
    return AddControl(L"STATIC", text, style, x, y, width, height, id);
}

HWND AddCombo(int x, int y, int width, int id) {
    return AddControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP |
        WS_VSCROLL, x, y, width, 300, id);
}

HWND AddTrack(int x, int y, int width, int id, int minimum, int maximum) {
    HWND track = AddControl(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS |
        WS_TABSTOP, x, y, width, 28, id);
    SendMessageW(track, TBM_SETRANGE, TRUE, MAKELONG(minimum, maximum));
    SendMessageW(track, TBM_SETLINESIZE, 0, 1);
    SendMessageW(track, TBM_SETPAGESIZE, 0, 5);
    return track;
}

void ComboAdd(HWND combo, const wchar_t* text) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void SetStatus(const std::wstring& text, bool error = false) {
    SetWindowTextW(Item(IdStatus), text.c_str());
    if (error) MessageBeep(MB_ICONWARNING);
}

std::wstring FormatFloat(float value) {
    wchar_t text[32]{};
    swprintf_s(text, L"%.2f", value);
    return text;
}

void UpdateTrackLabels() {
    SetWindowTextW(Item(IdConvergenceValue),
        std::to_wstring(static_cast<int>(SendMessageW(
            Item(IdConvergence), TBM_GETPOS, 0, 0))).c_str());
    SetWindowTextW(Item(IdPresentationScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdPresentationScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdHudHorizontalScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdHudHorizontalScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdHudVerticalScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdHudVerticalScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdMenuScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdMenuScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdCinemaScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdCinemaScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdNearViewValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdNearView), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
}

void UpdateModeControls() {
    const int selected = static_cast<int>(SendMessageW(Item(IdMode), CB_GETCURSEL, 0, 0));
    const bool dlss = selected >= 0 && w3vr::ModeUsesDlss(static_cast<RenderMode>(selected));
    EnableWindow(Item(IdDlssQuality), dlss);
}

void UpdateFirstPersonControls() {
    const bool enabled = SendMessageW(Item(IdFirstPersonGamepadHeadFollow),
        BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(Item(IdFirstPersonSnapTurnDegrees), enabled);
}

int SnapTurnIndexFor(int degrees) {
    if (degrees == 30) return 0;
    if (degrees == 60) return 2;
    return 1;
}

int SnapTurnDegreesFor(int index) {
    constexpr std::array<int, 3> degrees{{30, 45, 60}};
    return index >= 0 && index < static_cast<int>(degrees.size())
        ? degrees[static_cast<size_t>(index)]
        : 45;
}

void SetEditInteger(HWND edit, int value) {
    SetWindowTextW(edit, std::to_wstring(value).c_str());
}

void ApplyResolutionPreset(int selection) {
    constexpr std::array<std::pair<int, int>, 3> presets{{
        {2496, 2592}, {2688, 2784}, {3072, 3216}}};
    const bool custom = selection == 3;
    if (selection >= 0 && selection < 3) {
        SetEditInteger(Item(IdWidth), presets[selection].first);
        SetEditInteger(Item(IdHeight), presets[selection].second);
    }
    EnableWindow(Item(IdWidth), custom);
    EnableWindow(Item(IdHeight), custom);
}

int ResolutionPresetFor(int width, int height) {
    if (width == 2496 && height == 2592) return 0;
    if (width == 2688 && height == 2784) return 1;
    if (width == 3072 && height == 3216) return 2;
    return 3;
}

bool ReadInteger(HWND edit, int& result) {
    wchar_t text[32]{};
    GetWindowTextW(edit, text, static_cast<int>(std::size(text)));
    wchar_t* end{};
    const long value = wcstol(text, &end, 10);
    while (end && *end == L' ') ++end;
    if (end == text || (end && *end != L'\0')) return false;
    result = static_cast<int>(value);
    return true;
}

bool CaptureState(LauncherState& state, std::wstring& error) {
    const int mode = static_cast<int>(SendMessageW(Item(IdMode), CB_GETCURSEL, 0, 0));
    if (mode < 0 || mode > 5) {
        error = L"Select a render mode.";
        return false;
    }
    state.mode = static_cast<RenderMode>(mode);
    if (!ReadInteger(Item(IdWidth), state.width) ||
        !ReadInteger(Item(IdHeight), state.height) ||
        state.width < 640 || state.width > 8192 ||
        state.height < 640 || state.height > 8192) {
        error = L"Resolution must contain whole numbers between 640 and 8192.";
        return false;
    }
    state.dlss_quality = std::clamp(static_cast<int>(SendMessageW(
        Item(IdDlssQuality), CB_GETCURSEL, 0, 0)), 0, 4);
    state.hud_convergence_delta = static_cast<int>(SendMessageW(
        Item(IdConvergence), TBM_GETPOS, 0, 0));
    state.presentation_scale = static_cast<float>(SendMessageW(
        Item(IdPresentationScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.hud_horizontal_scale = static_cast<float>(SendMessageW(
        Item(IdHudHorizontalScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.hud_vertical_scale = static_cast<float>(SendMessageW(
        Item(IdHudVerticalScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.menu_scale = static_cast<float>(SendMessageW(
        Item(IdMenuScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_scale = static_cast<float>(SendMessageW(
        Item(IdCinemaScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.near_view = static_cast<float>(SendMessageW(
        Item(IdNearView), TBM_GETPOS, 0, 0)) / 100.0f;
    state.vertical_pitch_enabled = SendMessageW(
        Item(IdVerticalPitch), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.first_person_gamepad_head_follow = SendMessageW(
        Item(IdFirstPersonGamepadHeadFollow), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.first_person_snap_turn_degrees = SnapTurnDegreesFor(
        static_cast<int>(SendMessageW(
            Item(IdFirstPersonSnapTurnDegrees), CB_GETCURSEL, 0, 0)));
    state.first_person_combat_exit = SendMessageW(
        Item(IdFirstPersonCombatExit), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.cinema_5x4 = SendMessageW(
        Item(IdCinema5x4), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.steady_icons = SendMessageW(
        Item(IdSteadyIcons), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.diagnostic_logging = SendMessageW(
        Item(IdDiagnosticLogging), BM_GETCHECK, 0, 0) == BST_CHECKED;
    return true;
}

bool IsGameRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{sizeof(entry)};
    bool found{};
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"witcher3.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool LaunchGame(std::wstring& error) {
    if (GetFileAttributesW(g_app.paths.game_executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = L"witcher3.exe was not found next to the launcher.";
        return false;
    }
    std::wstring command = L"\"" + g_app.paths.game_executable.wstring() + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(g_app.paths.game_executable.c_str(), command.data(),
            nullptr, nullptr, FALSE, 0, nullptr,
            g_app.paths.launcher_directory.c_str(), &startup, &process)) {
        error = L"Could not start witcher3.exe (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

std::optional<std::string> LoadTextResource(int resource_id) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return std::nullopt;
    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) return std::nullopt;
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(module, resource);
    if (!data || size == 0) return std::nullopt;
    return std::string(static_cast<const char*>(data), size);
}

std::optional<std::string> LoadVrSettingsTemplate() {
    return LoadTextResource(IDR_VR_SETTINGS);
}

void ConfigureSettingsForVr() {
    if (IsGameRunning()) {
        MessageBoxW(g_app.window,
            L"Close The Witcher 3 before replacing its settings.",
            L"Witcher 3 VR Launcher", MB_OK | MB_ICONWARNING);
        return;
    }
    LauncherState state;
    std::wstring error;
    if (!CaptureState(state, error)) {
        MessageBoxW(g_app.window, error.c_str(), L"Invalid settings",
            MB_OK | MB_ICONWARNING);
        return;
    }
    const auto profile = LoadVrSettingsTemplate();
    if (!profile) {
        MessageBoxW(g_app.window, L"The embedded VR settings profile is missing.",
            L"Configuration failed", MB_OK | MB_ICONERROR);
        return;
    }
    if (!w3vr::ConfigureGameSettingsForVr(g_app.paths, *profile, error) ||
        !w3vr::SaveConfiguration(g_app.paths, state, error)) {
        MessageBoxW(g_app.window, error.c_str(), L"Configuration failed",
            MB_OK | MB_ICONERROR);
        return;
    }
    EnableWindow(Item(IdRestoreOriginal), TRUE);
    SetStatus(L"Complete VR baseline installed. Selected mode and resolution reapplied.");
}

void RestoreOriginalSettings() {
    if (IsGameRunning()) {
        MessageBoxW(g_app.window,
            L"Close The Witcher 3 before restoring its settings.",
            L"Witcher 3 VR Launcher", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring error;
    if (!w3vr::RestoreOriginalGameSettings(g_app.paths, error)) {
        MessageBoxW(g_app.window, error.c_str(), L"Restore failed",
            MB_OK | MB_ICONERROR);
        return;
    }
    EnableWindow(Item(IdRestoreOriginal), FALSE);
    SetStatus(L"Original dx12user.settings restored.");
}

void ShowCompatibilityWarnings() {
    const auto warnings = w3vr::InspectCompatibilitySettings(g_app.paths);
    if (warnings.ray_tracing_enabled) {
        MessageBoxW(g_app.window,
            L"Ray Tracing is enabled.\n\n"
            L"Ray Tracing has not been fixed for VR and may cause incorrect "
            L"rendering or unstable headset motion. Disable Ray Tracing in "
            L"the game's graphics options before launching.",
            L"Unsupported VR setting: Ray Tracing",
            MB_OK | MB_ICONWARNING);
    }
    if (warnings.ssr_high) {
        MessageBoxW(g_app.window,
            L"Screen Space Reflections are set to High.\n\n"
            L"SSR High has not been fixed for VR. Set Screen Space "
            L"Reflections to Low or Off in the game's graphics options "
            L"before launching.",
            L"Unsupported VR setting: SSR High",
            MB_OK | MB_ICONWARNING);
    }
}

void RestoreLauncherDefaults() {
    LauncherState defaults;
    SendMessageW(Item(IdMode), CB_SETCURSEL,
        static_cast<int>(defaults.mode), 0);
    SendMessageW(Item(IdResolution), CB_SETCURSEL, 1, 0);
    SetEditInteger(Item(IdWidth), defaults.width);
    SetEditInteger(Item(IdHeight), defaults.height);
    EnableWindow(Item(IdWidth), FALSE);
    EnableWindow(Item(IdHeight), FALSE);
    SendMessageW(Item(IdDlssQuality), CB_SETCURSEL, defaults.dlss_quality, 0);
    SendMessageW(Item(IdConvergence), TBM_SETPOS, TRUE,
        defaults.hud_convergence_delta);
    SendMessageW(Item(IdPresentationScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.presentation_scale * 100.0f)));
    SendMessageW(Item(IdHudHorizontalScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.hud_horizontal_scale * 100.0f)));
    SendMessageW(Item(IdHudVerticalScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.hud_vertical_scale * 100.0f)));
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.cinema_scale * 100.0f)));
    SendMessageW(Item(IdNearView), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.near_view * 100.0f)));
    SendMessageW(Item(IdVerticalPitch), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(Item(IdFirstPersonGamepadHeadFollow), BM_SETCHECK,
        defaults.first_person_gamepad_head_follow
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFirstPersonSnapTurnDegrees), CB_SETCURSEL,
        SnapTurnIndexFor(defaults.first_person_snap_turn_degrees), 0);
    SendMessageW(Item(IdFirstPersonCombatExit), BM_SETCHECK,
        defaults.first_person_combat_exit ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinema5x4), BM_SETCHECK,
        defaults.cinema_5x4 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdSteadyIcons), BM_SETCHECK,
        defaults.steady_icons ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdDiagnosticLogging), BM_SETCHECK, BST_UNCHECKED, 0);
    UpdateModeControls();
    UpdateFirstPersonControls();
    UpdateTrackLabels();
    SetStatus(L"Launcher defaults loaded. Press Save to apply them.");
}

void Save(bool launch) {
    if (IsGameRunning()) {
        MessageBoxW(g_app.window,
            L"Close The Witcher 3 before changing startup settings.",
            L"Witcher 3 VR Launcher", MB_OK | MB_ICONWARNING);
        return;
    }
    LauncherState state;
    std::wstring error;
    if (!CaptureState(state, error)) {
        MessageBoxW(g_app.window, error.c_str(), L"Invalid settings",
            MB_OK | MB_ICONWARNING);
        return;
    }
    if (!w3vr::SaveConfiguration(g_app.paths, state, error)) {
        MessageBoxW(g_app.window, error.c_str(), L"Save failed",
            MB_OK | MB_ICONERROR);
        return;
    }
    g_app.loaded = state;
    SetStatus(L"Settings saved. Backups use the .w3vr.bak suffix.");
    if (launch) {
        if (!LaunchGame(error)) {
            MessageBoxW(g_app.window, error.c_str(), L"Launch failed",
                MB_OK | MB_ICONERROR);
            return;
        }
        DestroyWindow(g_app.window);
    }
}

void PopulateControls() {
    HWND mode = Item(IdMode);
    for (int i = 0; i < 6; ++i) ComboAdd(mode,
        w3vr::ModeDisplayName(static_cast<RenderMode>(i)));

    HWND resolution = Item(IdResolution);
    ComboAdd(resolution, L"High - 2496 x 2592");
    ComboAdd(resolution, L"Ultra - 2688 x 2784");
    ComboAdd(resolution, L"Godlike - 3072 x 3216");
    ComboAdd(resolution, L"Custom");

    HWND quality = Item(IdDlssQuality);
    ComboAdd(quality, L"DLAA");
    ComboAdd(quality, L"Quality");
    ComboAdd(quality, L"Balanced");
    ComboAdd(quality, L"Performance");
    ComboAdd(quality, L"Ultra Performance");

    HWND snap_turn_degrees = Item(IdFirstPersonSnapTurnDegrees);
    ComboAdd(snap_turn_degrees, L"30 degrees");
    ComboAdd(snap_turn_degrees, L"45 degrees");
    ComboAdd(snap_turn_degrees, L"60 degrees");

    const auto loaded = w3vr::LoadConfiguration(g_app.paths);
    g_app.loaded = loaded.state;
    SendMessageW(mode, CB_SETCURSEL, static_cast<int>(loaded.state.mode), 0);
    const int preset = ResolutionPresetFor(loaded.state.width, loaded.state.height);
    SendMessageW(resolution, CB_SETCURSEL, preset, 0);
    SetEditInteger(Item(IdWidth), loaded.state.width);
    SetEditInteger(Item(IdHeight), loaded.state.height);
    EnableWindow(Item(IdWidth), preset == 3);
    EnableWindow(Item(IdHeight), preset == 3);
    SendMessageW(quality, CB_SETCURSEL, loaded.state.dlss_quality, 0);
    SendMessageW(Item(IdConvergence), TBM_SETPOS, TRUE,
        loaded.state.hud_convergence_delta);
    SendMessageW(Item(IdPresentationScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.presentation_scale * 100.0f)));
    SendMessageW(Item(IdHudHorizontalScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.hud_horizontal_scale * 100.0f)));
    SendMessageW(Item(IdHudVerticalScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.hud_vertical_scale * 100.0f)));
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.cinema_scale * 100.0f)));
    SendMessageW(Item(IdNearView), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.near_view * 100.0f)));
    SendMessageW(Item(IdVerticalPitch), BM_SETCHECK,
        loaded.state.vertical_pitch_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFirstPersonGamepadHeadFollow), BM_SETCHECK,
        loaded.state.first_person_gamepad_head_follow
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(snap_turn_degrees, CB_SETCURSEL,
        SnapTurnIndexFor(loaded.state.first_person_snap_turn_degrees), 0);
    SendMessageW(Item(IdFirstPersonCombatExit), BM_SETCHECK,
        loaded.state.first_person_combat_exit
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinema5x4), BM_SETCHECK,
        loaded.state.cinema_5x4 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdSteadyIcons), BM_SETCHECK,
        loaded.state.steady_icons ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdDiagnosticLogging), BM_SETCHECK,
        loaded.state.diagnostic_logging ? BST_CHECKED : BST_UNCHECKED, 0);
    UpdateModeControls();
    UpdateFirstPersonControls();
    UpdateTrackLabels();
    EnableWindow(Item(IdRestoreOriginal),
        w3vr::HasOriginalSettingsBackup(g_app.paths));
    SetStatus(loaded.warning.empty() ? L"Ready. Changes apply on the next game launch."
                                     : loaded.warning, !loaded.warning.empty());
}

void CreateInterface(HWND window) {
    g_app.window = window;
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    wcscpy_s(metrics.lfMessageFont.lfFaceName, L"Segoe UI");
    metrics.lfMessageFont.lfHeight = -15;
    g_app.font = CreateFontIndirectW(&metrics.lfMessageFont);

    AddControl(L"BUTTON", L"Startup rendering", BS_GROUPBOX,
        20, 18, 680, 104);
    AddLabel(L"Render mode", 38, 40, 150, 22);
    AddCombo(190, 36, 260, IdMode);
    AddLabel(L"DLSS preset", 470, 40, 100, 22);
    AddCombo(565, 36, 125, IdDlssQuality);
    AddLabel(L"Resolution", 38, 80, 150, 22);
    AddCombo(190, 76, 260, IdResolution);
    AddControl(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER | WS_TABSTOP,
        485, 76, 82, 25, IdWidth, WS_EX_CLIENTEDGE);
    AddLabel(L"x", 571, 80, 15, 22, 0, SS_CENTER);
    AddControl(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER | WS_TABSTOP,
        589, 76, 82, 25, IdHeight, WS_EX_CLIENTEDGE);

    AddControl(L"BUTTON", L"Comfort and interface", BS_GROUPBOX,
        20, 138, 680, 650);
    AddLabel(L"Presentation size", 38, 166, 170, 22);
    AddTrack(205, 160, 405, IdPresentationScale, 50, 100);
    AddLabel(L"1.00", 625, 166, 54, 22, IdPresentationScaleValue, SS_RIGHT);

    AddLabel(L"HUD convergence", 38, 214, 170, 22);
    AddTrack(205, 208, 405, IdConvergence, -64, 64);
    AddLabel(L"0", 625, 214, 54, 22, IdConvergenceValue, SS_RIGHT);

    AddLabel(L"HUD X zoom", 38, 264, 170, 22);
    AddTrack(205, 258, 405, IdHudHorizontalScale, 25, 100);
    AddLabel(L"0.50", 625, 264, 54, 22, IdHudHorizontalScaleValue, SS_RIGHT);

    AddLabel(L"HUD Y zoom", 38, 314, 170, 22);
    AddTrack(205, 308, 405, IdHudVerticalScale, 25, 100);
    AddLabel(L"0.50", 625, 314, 54, 22, IdHudVerticalScaleValue, SS_RIGHT);

    AddLabel(L"Menu window size", 38, 364, 170, 22);
    AddTrack(205, 358, 405, IdMenuScale, 30, 150);
    AddLabel(L"0.90", 625, 364, 54, 22, IdMenuScaleValue, SS_RIGHT);

    AddLabel(L"Cinema size", 38, 414, 170, 22);
    AddTrack(205, 408, 405, IdCinemaScale, 30, 150);
    AddLabel(L"0.90", 625, 414, 54, 22, IdCinemaScaleValue, SS_RIGHT);

    AddLabel(L"Near View", 38, 464, 170, 22);
    AddTrack(205, 458, 405, IdNearView, -200, 300);
    AddLabel(L"0.75", 625, 464, 54, 22, IdNearViewValue, SS_RIGHT);

    AddControl(L"BUTTON", L"Extended Cinema Framing (5:4)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 520, 410, 26, IdCinema5x4);
    AddLabel(L"Shows more of the scene during cinematics. HUD elements may appear slightly smaller.",
        58, 546, 610, 34);

    AddControl(L"BUTTON", L"Steady Icons (adds 1 frame latency)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 586, 410, 26, IdSteadyIcons);

    AddControl(L"BUTTON", L"Enable vertical mouse/pad pitch (Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 622, 410, 28, IdVerticalPitch);

    AddControl(L"BUTTON",
        L"Gamepad Snap Turn + Head Follow (First Person, Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 656, 455, 28,
        IdFirstPersonGamepadHeadFollow);
    AddLabel(L"Angle", 500, 660, 50, 22);
    AddCombo(550, 652, 125, IdFirstPersonSnapTurnDegrees);

    AddControl(L"BUTTON",
        L"Auto switch to third person during combats (First Person Only, experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 690, 630, 28,
        IdFirstPersonCombatExit);

    AddControl(L"BUTTON", L"Diagnostic Logging",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 726, 410, 26, IdDiagnosticLogging);
    AddLabel(L"Saves witcher3vr.log in the game folder for debugging. May affect performance.",
        58, 752, 610, 34);

    AddControl(L"BUTTON", L"Bindings", BS_GROUPBOX,
        20, 796, 680, 74);
    AddLabel(L"F8  Standard / Near    F9  Recenter    F10  Cinema    F11  First Person (Experimental)",
        38, 826, 650, 24);

    AddLabel(L"", 20, 880, 680, 38, IdStatus, SS_LEFT);
    AddControl(L"BUTTON", L"Configure Settings for VR", BS_PUSHBUTTON | WS_TABSTOP,
        20, 922, 220, 36, IdConfigureVr);
    AddControl(L"BUTTON", L"Restore Original Settings", BS_PUSHBUTTON | WS_TABSTOP,
        252, 922, 220, 36, IdRestoreOriginal);
    AddControl(L"BUTTON", L"Restore Defaults", BS_PUSHBUTTON | WS_TABSTOP,
        484, 922, 216, 36, IdRestoreDefaults);
    AddControl(L"BUTTON", L"Save Only", BS_PUSHBUTTON | WS_TABSTOP,
        406, 972, 130, 36, IdSave);
    AddControl(L"BUTTON", L"Save && Launch", BS_DEFPUSHBUTTON | WS_TABSTOP,
        550, 972, 150, 36, IdSaveLaunch);

    PopulateControls();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        CreateInterface(window);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int notification = HIWORD(wparam);
        if (id == IdMode && notification == CBN_SELCHANGE) UpdateModeControls();
        if (id == IdFirstPersonGamepadHeadFollow && notification == BN_CLICKED) {
            UpdateFirstPersonControls();
        }
        if (id == IdResolution && notification == CBN_SELCHANGE) {
            ApplyResolutionPreset(static_cast<int>(SendMessageW(
                Item(IdResolution), CB_GETCURSEL, 0, 0)));
        }
        if (id == IdSave && notification == BN_CLICKED) Save(false);
        if (id == IdSaveLaunch && notification == BN_CLICKED) Save(true);
        if (id == IdConfigureVr && notification == BN_CLICKED) ConfigureSettingsForVr();
        if (id == IdRestoreOriginal && notification == BN_CLICKED) RestoreOriginalSettings();
        if (id == IdRestoreDefaults && notification == BN_CLICKED) RestoreLauncherDefaults();
        return 0;
    }
    case WM_HSCROLL:
        UpdateTrackLabels();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_DESTROY:
        if (g_app.font) DeleteObject(g_app.font);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    g_app.paths = w3vr::DiscoverPaths();
    const auto default_ini = LoadTextResource(IDR_VR_INI);
    if (!default_ini) {
        MessageBoxW(nullptr, L"The embedded witcher3vr.ini template is missing.",
            L"Witcher 3 VR Launcher", MB_OK | MB_ICONERROR);
        return 3;
    }
    bool created_ini{};
    std::wstring configuration_error;
    if (!w3vr::EnsureVrConfiguration(
            g_app.paths, *default_ini, created_ini, configuration_error)) {
        MessageBoxW(nullptr, configuration_error.c_str(),
            L"Witcher 3 VR Launcher", MB_OK | MB_ICONERROR);
        return 4;
    }

    WNDCLASSEXW window_class{sizeof(window_class)};
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class)) return 1;

    RECT bounds{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRectEx(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        WS_MINIMIZEBOX, FALSE, 0);
    HWND window = CreateWindowExW(0, kWindowClass, L"Witcher 3 VR Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
        bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    ShowWindow(window, show);
    UpdateWindow(window);
    ShowCompatibilityWarnings();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
