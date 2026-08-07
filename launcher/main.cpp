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
#include <vector>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using w3vr::LauncherState;
using w3vr::RenderMode;

constexpr wchar_t kWindowClass[] = L"Witcher3VRLauncherWindow";
constexpr int kClientWidth = 720;
constexpr int kClientHeight = 1056;
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX;

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
    IdMenuScale,
    IdMenuScaleValue,
    IdCinemaScale,
    IdCinemaScaleValue,
    IdCinemaHudScale,
    IdCinemaHudScaleValue,
    IdCinemaHudConvergenceOffset,
    IdCinemaHudConvergenceOffsetValue,
    IdFullVrHudScale,
    IdFullVrHudScaleValue,
    IdFullVrHudConvergenceOffset,
    IdFullVrHudConvergenceOffsetValue,
    IdNearView,
    IdNearViewValue,
    IdVerticalPitch,
    IdFirstPersonGamepadHeadFollow,
    IdFirstPersonSnapTurnDegrees,
    IdFirstPersonCombatExit,
    IdFirstPersonStationaryTurn,
    IdFastMovementTransitions,
    IdCinemaFullVr,
    IdSteadyIcons,
    IdDiagnosticLogging,
    IdStatus,
    IdConfigureVr,
    IdRestoreOriginal,
    IdRestoreDefaults,
    IdSave,
    IdSaveLaunch,
};

struct ControlLayout {
    HWND window{};
    RECT design{};
};

struct App {
    HWND window{};
    HFONT font{};
    w3vr::ConfigPaths paths;
    LauncherState loaded;
    std::vector<ControlLayout> controls;
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
    if (control != nullptr) {
        g_app.controls.push_back(
            ControlLayout{control, RECT{x, y, x + width, y + height}});
    }
    return control;
}

void LayoutInterface() {
    if (g_app.window == nullptr || g_app.controls.empty()) return;

    RECT client{};
    GetClientRect(g_app.window, &client);
    const int client_width = std::max(client.right - client.left, 1L);
    const int client_height = std::max(client.bottom - client.top, 1L);
    const int layout_width = std::min(client_width, kClientWidth);
    const int layout_height = std::min(client_height, kClientHeight);

    HDWP positions = BeginDeferWindowPos(
        static_cast<int>(g_app.controls.size()));
    for (const auto& control : g_app.controls) {
        const int x = MulDiv(control.design.left, layout_width, kClientWidth);
        const int y = MulDiv(control.design.top, layout_height, kClientHeight);
        const int width = std::max(1, MulDiv(
            control.design.right - control.design.left,
            layout_width, kClientWidth));
        const int height = std::max(1, MulDiv(
            control.design.bottom - control.design.top,
            layout_height, kClientHeight));
        if (positions != nullptr) {
            positions = DeferWindowPos(
                positions, control.window, nullptr,
                x, y, width, height,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        } else {
            SetWindowPos(
                control.window, nullptr, x, y, width, height,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        }
    }
    if (positions != nullptr) EndDeferWindowPos(positions);
    RedrawWindow(g_app.window, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ALLCHILDREN);
}

RECT FitWindowToWorkArea(RECT window_rect) {
    const HMONITOR monitor = MonitorFromRect(
        &window_rect, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) return window_rect;

    const int work_width = info.rcWork.right - info.rcWork.left;
    const int work_height = info.rcWork.bottom - info.rcWork.top;
    const int requested_width = static_cast<int>(
        window_rect.right - window_rect.left);
    const int requested_height = static_cast<int>(
        window_rect.bottom - window_rect.top);
    const int width = std::min(requested_width, work_width);
    const int height = std::min(requested_height, work_height);
    const int x = info.rcWork.left + std::max(0, (work_width - width) / 2);
    const int y = info.rcWork.top + std::max(0, (work_height - height) / 2);
    return RECT{x, y, x + width, y + height};
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

void SetStatus(const std::wstring& text) {
    SetWindowTextW(Item(IdStatus), text.c_str());
}

std::wstring FormatFloat(float value) {
    wchar_t text[32]{};
    swprintf_s(text, L"%.2f", value);
    return text;
}

std::wstring FormatConvergenceOffset(int offset, int effective_shift) {
    wchar_t text[40]{};
    swprintf_s(text, L"%+d / %d", offset, effective_shift);
    return text;
}

void UpdateTrackLabels() {
    SetWindowTextW(Item(IdConvergenceValue),
        std::to_wstring(static_cast<int>(SendMessageW(
            Item(IdConvergence), TBM_GETPOS, 0, 0))).c_str());
    SetWindowTextW(Item(IdPresentationScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdPresentationScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdMenuScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdMenuScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    SetWindowTextW(Item(IdCinemaScaleValue), FormatFloat(
        static_cast<float>(SendMessageW(Item(IdCinemaScale), TBM_GETPOS, 0, 0)) / 100.0f).c_str());
    const float cinema_hud_scale = static_cast<float>(SendMessageW(
        Item(IdCinemaHudScale), TBM_GETPOS, 0, 0)) / 100.0f;
    const int cinema_offset = static_cast<int>(SendMessageW(
        Item(IdCinemaHudConvergenceOffset), TBM_GETPOS, 0, 0));
    SetWindowTextW(Item(IdCinemaHudScaleValue),
        FormatFloat(cinema_hud_scale).c_str());
    SetWindowTextW(Item(IdCinemaHudConvergenceOffsetValue),
        FormatConvergenceOffset(cinema_offset,
            w3vr::CinemaHudConvergenceShift(
                cinema_hud_scale, cinema_offset)).c_str());
    const float full_vr_hud_scale = static_cast<float>(SendMessageW(
        Item(IdFullVrHudScale), TBM_GETPOS, 0, 0)) / 100.0f;
    const int full_vr_offset = static_cast<int>(SendMessageW(
        Item(IdFullVrHudConvergenceOffset), TBM_GETPOS, 0, 0));
    SetWindowTextW(Item(IdFullVrHudScaleValue),
        FormatFloat(full_vr_hud_scale).c_str());
    SetWindowTextW(Item(IdFullVrHudConvergenceOffsetValue),
        FormatConvergenceOffset(full_vr_offset,
            w3vr::FullVrHudConvergenceShift(
                full_vr_hud_scale, full_vr_offset)).c_str());
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
    state.menu_scale = static_cast<float>(SendMessageW(
        Item(IdMenuScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_scale = static_cast<float>(SendMessageW(
        Item(IdCinemaScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_hud_scale = static_cast<float>(SendMessageW(
        Item(IdCinemaHudScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_hud_convergence_offset = static_cast<int>(SendMessageW(
        Item(IdCinemaHudConvergenceOffset), TBM_GETPOS, 0, 0));
    state.full_vr_hud_scale = static_cast<float>(SendMessageW(
        Item(IdFullVrHudScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.full_vr_hud_convergence_offset = static_cast<int>(SendMessageW(
        Item(IdFullVrHudConvergenceOffset), TBM_GETPOS, 0, 0));
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
    state.first_person_stationary_turn = SendMessageW(
        Item(IdFirstPersonStationaryTurn), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.fast_movement_transitions = SendMessageW(
        Item(IdFastMovementTransitions), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.cinema_full_vr = SendMessageW(
        Item(IdCinemaFullVr), BM_GETCHECK, 0, 0) == BST_CHECKED;
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

struct GameWindowSearch {
    DWORD process_id{};
    HWND window{};
};

BOOL CALLBACK FindGameWindowCallback(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<GameWindowSearch*>(parameter);
    DWORD process_id{};
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != search->process_id || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != nullptr ||
        (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) {
        return TRUE;
    }
    search->window = window;
    return FALSE;
}

HWND FindGameWindow(DWORD process_id) {
    GameWindowSearch search{process_id, nullptr};
    EnumWindows(&FindGameWindowCallback,
        reinterpret_cast<LPARAM>(&search));
    return search.window;
}

void FocusLaunchedGame(HANDLE process, DWORD process_id) {
    // The launcher owns foreground activation at this point, so explicitly
    // grant the child permission before its window exists. Steam/VR overlays
    // can otherwise leave the new Witcher window behind another application.
    AllowSetForegroundWindow(process_id);
    WaitForInputIdle(process, 15000);

    constexpr ULONGLONG kWindowWaitMilliseconds = 45000;
    const ULONGLONG deadline = GetTickCount64() + kWindowWaitMilliseconds;
    while (GetTickCount64() < deadline &&
        WaitForSingleObject(process, 0) != WAIT_OBJECT_0) {
        HWND window = FindGameWindow(process_id);
        if (window == nullptr) {
            Sleep(100);
            continue;
        }
        if (IsIconic(window)) {
            ShowWindowAsync(window, SW_RESTORE);
        }
        BringWindowToTop(window);
        SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        SetForegroundWindow(window);
        return;
    }
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
    FocusLaunchedGame(process.hProcess, process.dwProcessId);
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
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.cinema_scale * 100.0f)));
    SendMessageW(Item(IdCinemaHudScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.cinema_hud_scale * 100.0f)));
    SendMessageW(Item(IdCinemaHudConvergenceOffset), TBM_SETPOS, TRUE,
        defaults.cinema_hud_convergence_offset);
    SendMessageW(Item(IdFullVrHudScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.full_vr_hud_scale * 100.0f)));
    SendMessageW(Item(IdFullVrHudConvergenceOffset), TBM_SETPOS, TRUE,
        defaults.full_vr_hud_convergence_offset);
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
    SendMessageW(Item(IdFirstPersonStationaryTurn), BM_SETCHECK,
        defaults.first_person_stationary_turn ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFastMovementTransitions), BM_SETCHECK,
        defaults.fast_movement_transitions ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinemaFullVr), BM_SETCHECK,
        defaults.cinema_full_vr ? BST_CHECKED : BST_UNCHECKED, 0);
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
    if (const auto compatible_width =
            w3vr::DlssNearSquareCompatibleWidth(state)) {
        const std::wstring adjusted_resolution =
            std::to_wstring(*compatible_width) + L" x " +
            std::to_wstring(state.height);
        const std::wstring message =
            L"A REDengine bug prevents DLSS from working correctly with "
            L"square and near-square resolutions.\n\nThe resolution will be adjusted to " +
            adjusted_resolution +
            L".\n\nNo settings will be saved and the game will not launch yet. "
            L"Press Save or Save & Launch again to continue.";
        MessageBoxW(g_app.window, message.c_str(),
            L"DLSS resolution compatibility", MB_OK | MB_ICONINFORMATION);
        SendMessageW(Item(IdResolution), CB_SETCURSEL, 3, 0);
        SetEditInteger(Item(IdWidth), *compatible_width);
        SetEditInteger(Item(IdHeight), state.height);
        EnableWindow(Item(IdWidth), TRUE);
        EnableWindow(Item(IdHeight), TRUE);
        SetStatus(L"DLSS resolution adjusted. Nothing was saved; press Save again.");
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
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.cinema_scale * 100.0f)));
    SendMessageW(Item(IdCinemaHudScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.cinema_hud_scale * 100.0f)));
    SendMessageW(Item(IdCinemaHudConvergenceOffset), TBM_SETPOS, TRUE,
        loaded.state.cinema_hud_convergence_offset);
    SendMessageW(Item(IdFullVrHudScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.full_vr_hud_scale * 100.0f)));
    SendMessageW(Item(IdFullVrHudConvergenceOffset), TBM_SETPOS, TRUE,
        loaded.state.full_vr_hud_convergence_offset);
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
    SendMessageW(Item(IdFirstPersonStationaryTurn), BM_SETCHECK,
        loaded.state.first_person_stationary_turn
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFastMovementTransitions), BM_SETCHECK,
        loaded.state.fast_movement_transitions
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinemaFullVr), BM_SETCHECK,
        loaded.state.cinema_full_vr ? BST_CHECKED : BST_UNCHECKED, 0);
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
                                     : loaded.warning);
}

void CreateInterface(HWND window) {
    g_app.window = window;
    g_app.controls.clear();
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
        20, 138, 680, 658);
    AddLabel(L"Presentation size", 38, 166, 170, 22);
    AddTrack(205, 160, 405, IdPresentationScale, 50, 100);
    AddLabel(L"1.00", 625, 166, 54, 22, IdPresentationScaleValue, SS_RIGHT);

    AddLabel(L"HUD convergence", 38, 214, 170, 22);
    AddTrack(205, 208, 405, IdConvergence, -64, 64);
    AddLabel(L"0", 625, 214, 54, 22, IdConvergenceValue, SS_RIGHT);

    AddLabel(L"Menu window size", 38, 264, 170, 22);
    AddTrack(205, 258, 405, IdMenuScale, 30, 150);
    AddLabel(L"0.85", 625, 264, 54, 22, IdMenuScaleValue, SS_RIGHT);

    AddLabel(L"Cinema screen size", 38, 314, 170, 22);
    AddTrack(205, 308, 405, IdCinemaScale, 30, 150);
    AddLabel(L"0.90", 625, 314, 54, 22, IdCinemaScaleValue, SS_RIGHT);

    AddLabel(L"Near View", 38, 364, 170, 22);
    AddTrack(205, 358, 405, IdNearView, -200, 300);
    AddLabel(L"0.75", 625, 364, 54, 22, IdNearViewValue, SS_RIGHT);

    AddLabel(L"Cinema3D HUD/text size", 38, 414, 150, 22);
    AddTrack(188, 408, 112, IdCinemaHudScale, 50, 150);
    AddLabel(L"1.30", 302, 414, 46, 22, IdCinemaHudScaleValue, SS_RIGHT);
    AddLabel(L"Full VR HUD/text size", 365, 414, 145, 22);
    AddTrack(510, 408, 112, IdFullVrHudScale, 50, 150);
    AddLabel(L"0.75", 624, 414, 55, 22, IdFullVrHudScaleValue, SS_RIGHT);

    AddLabel(L"Cinema3D conv. offset", 38, 458, 150, 22);
    AddTrack(188, 452, 100, IdCinemaHudConvergenceOffset, -64, 64);
    AddLabel(L"+0 / -72", 292, 458, 62, 22,
        IdCinemaHudConvergenceOffsetValue, SS_RIGHT);
    AddLabel(L"Full VR conv. offset", 365, 458, 145, 22);
    AddTrack(510, 452, 100, IdFullVrHudConvergenceOffset, -64, 64);
    AddLabel(L"+0 / -36", 612, 458, 67, 22,
        IdFullVrHudConvergenceOffsetValue, SS_RIGHT);

    AddControl(L"BUTTON",
        L"Show Automatic Cutscenes in Full VR",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 494, 560, 26,
        IdCinemaFullVr);

    AddControl(L"BUTTON", L"Steady Icons (adds 1 frame latency)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 526, 410, 26, IdSteadyIcons);

    AddControl(L"BUTTON", L"Enable vertical mouse/pad pitch (Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 558, 410, 28, IdVerticalPitch);

    AddControl(L"BUTTON",
        L"Gamepad Snap Turn + Head Follow (First Person, Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 590, 455, 28,
        IdFirstPersonGamepadHeadFollow);
    AddLabel(L"Angle", 500, 594, 50, 22);
    AddCombo(550, 586, 125, IdFirstPersonSnapTurnDegrees);

    AddControl(L"BUTTON",
        L"Auto switch to third person during combats (First Person Only)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 624, 630, 28,
        IdFirstPersonCombatExit);

    AddControl(L"BUTTON",
        L"Turn body while stationary in First Person (Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 656, 590, 26,
        IdFirstPersonStationaryTurn);

    AddControl(L"BUTTON",
        L"Faster Movement Transitions",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 688, 520, 26,
        IdFastMovementTransitions);

    AddControl(L"BUTTON", L"Diagnostic Logging",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 720, 410, 26, IdDiagnosticLogging);
    AddLabel(L"Saves witcher3vr.log in the game folder for debugging. May affect performance.",
        58, 746, 610, 34);

    AddControl(L"BUTTON", L"Bindings", BS_GROUPBOX,
        20, 804, 680, 112);
    AddLabel(L"F8  Standard / Near    F9  Recenter    F10  Cinema    F11  First Person (Experimental)",
        38, 830, 650, 24);
    AddLabel(L"HUD editor: Insert open/close    Q/E select panel    Arrow keys move    Wheel scales",
        38, 854, 650, 24);
    AddLabel(L"Esc save/close    R reset panel    X reset profile    Tab switch VR / Cinema3D",
        38, 878, 650, 24);

    AddLabel(L"", 20, 924, 680, 30, IdStatus, SS_LEFT);
    AddControl(L"BUTTON", L"Configure Settings for VR", BS_PUSHBUTTON | WS_TABSTOP,
        20, 958, 220, 36, IdConfigureVr);
    AddControl(L"BUTTON", L"Restore Original Settings", BS_PUSHBUTTON | WS_TABSTOP,
        252, 958, 220, 36, IdRestoreOriginal);
    AddControl(L"BUTTON", L"Restore Defaults", BS_PUSHBUTTON | WS_TABSTOP,
        484, 958, 216, 36, IdRestoreDefaults);
    AddControl(L"BUTTON", L"Save Only", BS_PUSHBUTTON | WS_TABSTOP,
        406, 1008, 130, 36, IdSave);
    AddControl(L"BUTTON", L"Save && Launch", BS_DEFPUSHBUTTON | WS_TABSTOP,
        550, 1008, 150, 36, IdSaveLaunch);

    PopulateControls();
    LayoutInterface();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        CreateInterface(window);
        return 0;
    case WM_SIZE:
        LayoutInterface();
        return 0;
    case WM_DPICHANGED: {
        const auto suggested = reinterpret_cast<const RECT*>(lparam);
        if (suggested != nullptr) {
            const RECT fitted = FitWindowToWorkArea(*suggested);
            SetWindowPos(window, nullptr,
                fitted.left, fitted.top,
                fitted.right - fitted.left,
                fitted.bottom - fitted.top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        }
        return 0;
    }
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

    std::wstring hud_setup_error;
    if (!w3vr::EnsureHudEditorSetup(g_app.paths, hud_setup_error)) {
        MessageBoxW(nullptr, hud_setup_error.c_str(),
            L"Witcher 3 VR HUD Editor setup", MB_OK | MB_ICONWARNING);
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
    if (!AdjustWindowRectExForDpi(
            &bounds, kWindowStyle, FALSE, 0, GetDpiForSystem())) {
        AdjustWindowRectEx(&bounds, kWindowStyle, FALSE, 0);
    }
    bounds = FitWindowToWorkArea(bounds);
    HWND window = CreateWindowExW(0, kWindowClass, L"Witcher 3 VR Launcher",
        kWindowStyle,
        bounds.left, bounds.top, bounds.right - bounds.left,
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
