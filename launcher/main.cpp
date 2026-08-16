#include "config.h"
#include "resources.h"

#include <Windows.h>
#include <CommCtrl.h>
#include <Shellapi.h>
#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using w3vr::LauncherState;
using w3vr::RenderMode;
using w3vr::CinemaAspect;

constexpr wchar_t kWindowClass[] = L"Witcher3VRLauncherWindow";
constexpr int kClientWidth = 720;
constexpr int kClientHeight = 1120;
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX;

enum ControlId {
    IdMode = 100,
    IdResolution,
    IdWidth,
    IdHeight,
    IdDlssQuality,
    IdRayTracing,
    IdConvergence,
    IdConvergenceValue,
    IdPresentationScale,
    IdPresentationScaleValue,
    IdMenuScale,
    IdMenuScaleValue,
    IdCinemaScale,
    IdCinemaScaleValue,
    IdCinemaAspect,
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
    IdFirstPersonStrafe,
    IdFirstPersonAnchorSmoothing,
    IdFastMovementTransitions,
    IdCinemaFullVr,
    IdSteadyIcons,
    IdAlternatePresentationResize,
    IdNativeStereo,
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
    HWND tooltip{};
    HFONT font{};
    w3vr::ConfigPaths paths;
    LauncherState loaded;
    // Advanced INI-only option. Keep the loaded value across launcher saves
    // without exposing a UI control or silently disabling a manual opt-in.
    bool fullscreen_projection{};
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

void AddTooltip(HWND control, const wchar_t* text) {
    if (control == nullptr || g_app.tooltip == nullptr || text == nullptr) {
        return;
    }
    TOOLINFOW tool{sizeof(tool)};
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = g_app.window;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<wchar_t*>(text);
    SendMessageW(g_app.tooltip, TTM_ADDTOOLW, 0,
        reinterpret_cast<LPARAM>(&tool));
}

void AddTooltips(const wchar_t* text, std::initializer_list<HWND> controls) {
    for (HWND control : controls) {
        AddTooltip(control, text);
    }
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
    const bool selected_valid = selected >= 0 &&
        selected < static_cast<int>(RenderMode::Count);
    const auto selected_mode = selected_valid
        ? static_cast<RenderMode>(selected)
        : RenderMode::StereoNone;
    const bool dlss = selected_valid && w3vr::ModeUsesDlss(selected_mode);
    EnableWindow(Item(IdDlssQuality), dlss);
    const bool ray_tracing_available = selected_valid &&
        w3vr::ModeSupportsRayTracing(selected_mode);
    if (!ray_tracing_available) {
        SendMessageW(Item(IdRayTracing), BM_SETCHECK, BST_UNCHECKED, 0);
    }
    EnableWindow(Item(IdRayTracing), ray_tracing_available);
    const bool native_stereo_available = selected_valid &&
        w3vr::ModeUsesStereo(selected_mode);
    EnableWindow(Item(IdNativeStereo), native_stereo_available);
    const bool native_stereo_active = native_stereo_available &&
        SendMessageW(Item(IdNativeStereo), BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(Item(IdPresentationScale), TRUE);
    const float presentation_scale = static_cast<float>(SendMessageW(
        Item(IdPresentationScale), TBM_GETPOS, 0, 0)) / 100.0f;
    const bool alternate_resize_available = selected_valid &&
        w3vr::AlternatePresentationResizeAvailable(
            selected_mode, native_stereo_active, presentation_scale);
    if (!alternate_resize_available) {
        SendMessageW(Item(IdAlternatePresentationResize),
            BM_SETCHECK, BST_UNCHECKED, 0);
    }
    EnableWindow(Item(IdAlternatePresentationResize),
        alternate_resize_available);
    UpdateTrackLabels();
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
    if (mode < 0 || mode >= static_cast<int>(RenderMode::Count)) {
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
    state.ray_tracing = SendMessageW(
        Item(IdRayTracing), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.hud_convergence_delta = static_cast<int>(SendMessageW(
        Item(IdConvergence), TBM_GETPOS, 0, 0));
    state.presentation_scale = static_cast<float>(SendMessageW(
        Item(IdPresentationScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.menu_scale = static_cast<float>(SendMessageW(
        Item(IdMenuScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_scale = static_cast<float>(SendMessageW(
        Item(IdCinemaScale), TBM_GETPOS, 0, 0)) / 100.0f;
    state.cinema_aspect = static_cast<CinemaAspect>(std::clamp(
        static_cast<int>(SendMessageW(
            Item(IdCinemaAspect), CB_GETCURSEL, 0, 0)), 0, 1));
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
    state.first_person_strafe = SendMessageW(
        Item(IdFirstPersonStrafe), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.first_person_anchor_smoothing = SendMessageW(
        Item(IdFirstPersonAnchorSmoothing), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.fast_movement_transitions = SendMessageW(
        Item(IdFastMovementTransitions), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.cinema_full_vr = SendMessageW(
        Item(IdCinemaFullVr), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.steady_icons = SendMessageW(
        Item(IdSteadyIcons), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.alternate_presentation_resize = SendMessageW(
        Item(IdAlternatePresentationResize), BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.fullscreen_projection = state.alternate_presentation_resize ||
        (g_app.fullscreen_projection &&
            !g_app.loaded.alternate_presentation_resize);
    state.native_stereo = SendMessageW(
        Item(IdNativeStereo), BM_GETCHECK, 0, 0) == BST_CHECKED;
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

std::optional<std::filesystem::path> WriteHudEditorManualSetupGuide() {
    std::array<wchar_t, 32768> temporary_directory{};
    const DWORD length = GetTempPathW(
        static_cast<DWORD>(temporary_directory.size()),
        temporary_directory.data());
    if (length == 0 || length >= temporary_directory.size()) {
        return std::nullopt;
    }

    const auto path = std::filesystem::path(temporary_directory.data()) /
        L"Witcher3VR-HUD-Editor-Manual-Setup.txt";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    const std::wstring instructions =
        w3vr::HudEditorManualSetupInstructions(g_app.paths);
    const wchar_t bom = static_cast<wchar_t>(0xFEFF);
    DWORD bom_written{};
    DWORD contents_written{};
    const bool size_valid = instructions.size() <=
        MAXDWORD / sizeof(wchar_t);
    const DWORD contents_size = size_valid
        ? static_cast<DWORD>(instructions.size() * sizeof(wchar_t))
        : 0;
    const bool written = size_valid &&
        WriteFile(file, &bom, sizeof(bom), &bom_written, nullptr) &&
        bom_written == sizeof(bom) &&
        WriteFile(file, instructions.data(), contents_size,
            &contents_written, nullptr) &&
        contents_written == contents_size &&
        FlushFileBuffers(file);
    CloseHandle(file);
    if (!written) {
        DeleteFileW(path.c_str());
        return std::nullopt;
    }
    return path;
}

void ShowHudEditorSetupFailure(HWND owner, const std::wstring& error) {
    const auto guide = WriteHudEditorManualSetupGuide();
    std::wstring message = error;
    message +=
        L"\n\nAutomatic HUD Editor setup was not confirmed. Close The "
        L"Witcher 3 before retrying; the game can overwrite input.settings "
        L"when it exits.";
    if (guide) {
        message += L"\n\nA manual setup guide was created at:\n" +
            guide->wstring() + L"\n\nIt will open after this warning.";
    } else {
        message += L"\n\n" +
            w3vr::HudEditorManualSetupInstructions(g_app.paths);
    }
    MessageBoxW(owner, message.c_str(),
        L"Witcher 3 VR HUD Editor setup", MB_OK | MB_ICONWARNING);
    if (guide) {
        ShellExecuteW(owner, L"open", guide->c_str(), nullptr, nullptr,
            SW_SHOWNORMAL);
    }
}

bool EnsureHudEditorReady(HWND owner) {
    std::wstring error;
    if (IsGameRunning()) {
        error = L"The Witcher 3 is currently running. HUD Editor setup was "
            L"not attempted because the game could overwrite input.settings "
            L"when it exits.";
    } else if (w3vr::EnsureHudEditorSetup(g_app.paths, error)) {
        return true;
    }
    ShowHudEditorSetupFailure(owner, error);
    return false;
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
    if (warnings.ray_tracing_enabled &&
        !w3vr::ModeSupportsRayTracing(g_app.loaded.mode)) {
        MessageBoxW(g_app.window,
            L"Ray Tracing is currently enabled in the game with an "
            L"incompatible render mode.\n\nThe launcher now controls both the "
            L"game setting and the Witcher 3 VR flag. Save or Save & Launch "
            L"will turn Ray Tracing off unless an AER + AFW TAAU or DLSS "
            L"mode is selected.",
            L"Ray Tracing mode compatibility",
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
    SendMessageW(Item(IdRayTracing), BM_SETCHECK,
        defaults.ray_tracing ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdConvergence), TBM_SETPOS, TRUE,
        defaults.hud_convergence_delta);
    SendMessageW(Item(IdPresentationScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.presentation_scale * 100.0f)));
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(defaults.cinema_scale * 100.0f)));
    SendMessageW(Item(IdCinemaAspect), CB_SETCURSEL,
        static_cast<int>(defaults.cinema_aspect), 0);
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
    SendMessageW(Item(IdFirstPersonStrafe), BM_SETCHECK,
        defaults.first_person_strafe ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFirstPersonAnchorSmoothing), BM_SETCHECK,
        defaults.first_person_anchor_smoothing ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFastMovementTransitions), BM_SETCHECK,
        defaults.fast_movement_transitions ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinemaFullVr), BM_SETCHECK,
        defaults.cinema_full_vr ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdSteadyIcons), BM_SETCHECK,
        defaults.steady_icons ? BST_CHECKED : BST_UNCHECKED, 0);
    g_app.fullscreen_projection = defaults.fullscreen_projection;
    SendMessageW(Item(IdAlternatePresentationResize), BM_SETCHECK,
        defaults.alternate_presentation_resize ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdNativeStereo), BM_SETCHECK,
        defaults.native_stereo ? BST_CHECKED : BST_UNCHECKED, 0);
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
    if (!EnsureHudEditorReady(g_app.window)) {
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
    g_app.fullscreen_projection = state.fullscreen_projection;
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
    for (int i = 0; i < static_cast<int>(RenderMode::Count); ++i) ComboAdd(mode,
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

    HWND cinema_aspect = Item(IdCinemaAspect);
    ComboAdd(cinema_aspect, L"5:4");
    ComboAdd(cinema_aspect, L"4:3");

    HWND snap_turn_degrees = Item(IdFirstPersonSnapTurnDegrees);
    ComboAdd(snap_turn_degrees, L"30 degrees");
    ComboAdd(snap_turn_degrees, L"45 degrees");
    ComboAdd(snap_turn_degrees, L"60 degrees");

    const auto loaded = w3vr::LoadConfiguration(g_app.paths);
    g_app.loaded = loaded.state;
    g_app.fullscreen_projection = loaded.state.fullscreen_projection;
    SendMessageW(mode, CB_SETCURSEL, static_cast<int>(loaded.state.mode), 0);
    const int preset = ResolutionPresetFor(loaded.state.width, loaded.state.height);
    SendMessageW(resolution, CB_SETCURSEL, preset, 0);
    SetEditInteger(Item(IdWidth), loaded.state.width);
    SetEditInteger(Item(IdHeight), loaded.state.height);
    EnableWindow(Item(IdWidth), preset == 3);
    EnableWindow(Item(IdHeight), preset == 3);
    SendMessageW(quality, CB_SETCURSEL, loaded.state.dlss_quality, 0);
    SendMessageW(Item(IdRayTracing), BM_SETCHECK,
        loaded.state.ray_tracing ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdConvergence), TBM_SETPOS, TRUE,
        loaded.state.hud_convergence_delta);
    SendMessageW(Item(IdPresentationScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.presentation_scale * 100.0f)));
    SendMessageW(Item(IdMenuScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.menu_scale * 100.0f)));
    SendMessageW(Item(IdCinemaScale), TBM_SETPOS, TRUE,
        static_cast<int>(std::lround(loaded.state.cinema_scale * 100.0f)));
    SendMessageW(cinema_aspect, CB_SETCURSEL,
        static_cast<int>(loaded.state.cinema_aspect), 0);
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
    SendMessageW(Item(IdFirstPersonStrafe), BM_SETCHECK,
        loaded.state.first_person_strafe
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFirstPersonAnchorSmoothing), BM_SETCHECK,
        loaded.state.first_person_anchor_smoothing
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdFastMovementTransitions), BM_SETCHECK,
        loaded.state.fast_movement_transitions
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdCinemaFullVr), BM_SETCHECK,
        loaded.state.cinema_full_vr ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdSteadyIcons), BM_SETCHECK,
        loaded.state.steady_icons ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdAlternatePresentationResize), BM_SETCHECK,
        loaded.state.alternate_presentation_resize
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Item(IdNativeStereo), BM_SETCHECK,
        loaded.state.native_stereo ? BST_CHECKED : BST_UNCHECKED, 0);
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

    g_app.tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        window, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetWindowPos(g_app.tooltip, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(g_app.tooltip, TTM_SETMAXTIPWIDTH, 0, 460);
    SendMessageW(g_app.tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 350);
    SendMessageW(g_app.tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);

    AddTooltip(AddControl(L"BUTTON", L"Startup rendering", BS_GROUPBOX,
        20, 18, 680, 142),
        L"Select the stereo producer, headset render resolution, DLSS quality, and the safe Ray Tracing route used at game startup.");
    AddTooltips(
        L"AER + AFW uses PureDark AFW to generate the missing eye from alternating real eyes. It is the highest-performance route with minimal artifacts. Stereo renders both eyes and is steadier, but costs more GPU time.",
        {AddLabel(L"Render mode", 38, 40, 150, 22),
         AddCombo(190, 36, 260, IdMode)});
    AddTooltips(
        L"Select DLAA or the DLSS quality/performance preset. This control is available only for DLSS render modes.",
        {AddLabel(L"DLSS preset", 470, 40, 100, 22),
         AddCombo(565, 36, 125, IdDlssQuality)});
    AddTooltips(
        L"Set the render resolution recommended for your own headset. High, Ultra, and Godlike are presets only for Quest 3 with Virtual Desktop; use Custom for other headsets or runtimes.",
        {AddLabel(L"Resolution", 38, 80, 150, 22),
         AddCombo(190, 76, 260, IdResolution),
         AddControl(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER |
             WS_TABSTOP, 485, 76, 82, 25, IdWidth, WS_EX_CLIENTEDGE),
         AddLabel(L"x", 571, 80, 15, 22, 0, SS_CENTER),
         AddControl(L"EDIT", L"", WS_BORDER | ES_NUMBER | ES_CENTER |
             WS_TABSTOP, 589, 76, 82, 25, IdHeight, WS_EX_CLIENTEDGE)});
    AddTooltip(AddControl(L"BUTTON",
        L"Ray Tracing (AER + AFW)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 112, 620, 28, IdRayTracing),
        L"The launcher controls both the game's Ray Tracing master switch and Witcher 3 VR's RT flag. Ray Tracing can be enabled with AER + AFW using either TAAU or DLSS, and is forced off for every other render mode.");

    AddTooltip(AddControl(L"BUTTON", L"Comfort and interface", BS_GROUPBOX,
        20, 174, 680, 494),
        L"Tune the headset presentation, HUD, cinema framing, and experimental renderer options. Hover any setting name or control for details.");
    AddTooltips(
        L"Lower values concentrate the same render resolution into a smaller angular area, increasing effective pixel density and supersampling. Black bands gradually appear, so find the lowest value that remains comfortable for your headset and fit.",
        {AddLabel(L"Presentation size", 38, 200, 170, 22),
         AddTrack(205, 194, 405, IdPresentationScale, 50, 100),
         AddLabel(L"1.00", 625, 200, 54, 22,
             IdPresentationScaleValue, SS_RIGHT)});

    AddTooltips(
        L"Adjust the stereo depth of gameplay HUD elements. Move it only enough to make the interface comfortable to focus on.",
        {AddLabel(L"HUD convergence", 38, 244, 170, 22),
         AddTrack(205, 238, 405, IdConvergence, -64, 64),
         AddLabel(L"0", 625, 244, 54, 22,
             IdConvergenceValue, SS_RIGHT)});

    AddTooltips(
        L"Changes the size of the floating menu window without changing its distance.",
        {AddLabel(L"Menu window size", 38, 288, 170, 22),
         AddTrack(205, 282, 405, IdMenuScale, 30, 150),
         AddLabel(L"0.85", 625, 288, 54, 22,
             IdMenuScaleValue, SS_RIGHT)});

    AddTooltips(
        L"Changes the size of the anchored Cinema3D screen used by menus, videos, and non-Full-VR scenes.",
        {AddLabel(L"Cinema screen size", 38, 332, 170, 22),
         AddTrack(205, 326, 235, IdCinemaScale, 30, 150),
         AddLabel(L"0.90", 445, 332, 50, 22,
             IdCinemaScaleValue, SS_RIGHT)});
    AddTooltips(
        L"Select the Cinema3D screen aspect ratio. 5:4 is the recommended default; 4:3 is available for personal preference.",
        {AddLabel(L"Aspect", 510, 332, 58, 22),
         AddCombo(575, 324, 104, IdCinemaAspect)});

    AddTooltips(
        L"Adjusts the close third-person camera preset selected with F8. Higher values move the camera farther from Geralt.",
        {AddLabel(L"Near View", 38, 376, 170, 22),
         AddTrack(205, 370, 405, IdNearView, -200, 300),
         AddLabel(L"0.75", 625, 376, 54, 22,
             IdNearViewValue, SS_RIGHT)});

    AddTooltips(
        L"Changes HUD and subtitle size on the Cinema3D screen.",
        {AddLabel(L"Cinema3D HUD/text size", 38, 420, 150, 22),
         AddTrack(188, 414, 112, IdCinemaHudScale, 50, 150),
         AddLabel(L"1.30", 302, 420, 46, 22,
             IdCinemaHudScaleValue, SS_RIGHT)});
    AddTooltips(
        L"Changes HUD and subtitle size when the scene is rendered in Full VR.",
        {AddLabel(L"Full VR HUD/text size", 365, 420, 145, 22),
         AddTrack(510, 414, 112, IdFullVrHudScale, 50, 150),
         AddLabel(L"1.00", 624, 420, 55, 22,
             IdFullVrHudScaleValue, SS_RIGHT)});

    AddTooltips(
        L"Fine-tunes Cinema3D HUD depth around the automatic convergence calculated from HUD size.",
        {AddLabel(L"Cinema3D conv. offset", 38, 462, 150, 22),
         AddTrack(188, 456, 100, IdCinemaHudConvergenceOffset, -64, 64),
         AddLabel(L"+0 / -72", 292, 462, 62, 22,
             IdCinemaHudConvergenceOffsetValue, SS_RIGHT)});
    AddTooltips(
        L"Fine-tunes Full VR HUD depth without changing the physical depth maintained when HUD size changes.",
        {AddLabel(L"Full VR conv. offset", 365, 462, 145, 22),
         AddTrack(510, 456, 100, IdFullVrHudConvergenceOffset, -64, 64),
         AddLabel(L"+0 / -36", 612, 462, 67, 22,
             IdFullVrHudConvergenceOffsetValue, SS_RIGHT)});

    AddTooltip(AddControl(L"BUTTON", L"Show Automatic Cutscenes in Full VR",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 498, 320, 26, IdCinemaFullVr),
        L"Keeps supported automatic cutscenes in geometry stereo Full VR instead of placing them on the Cinema3D screen.");
    AddTooltip(AddControl(L"BUTTON", L"Steady Icons",
        BS_AUTOCHECKBOX | WS_TABSTOP, 365, 498, 285, 26, IdSteadyIcons),
        L"Stabilizes world-space icons. It adds one frame only in Stereo; under AER + AFW it may not remain as stable as it does in Stereo.");

    AddTooltip(AddControl(L"BUTTON",
        L"Enable vertical mouse/pad pitch (Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 530, 335, 28, IdVerticalPitch),
        L"Allows mouse or gamepad pitch to tilt the camera vertically. The headset currently moves incorrectly when the view is not level with the horizon; this issue is not fixed yet.");
    AddTooltip(AddControl(L"BUTTON", L"Faster Movement Transitions",
        BS_AUTOCHECKBOX | WS_TABSTOP, 380, 530, 285, 26,
        IdFastMovementTransitions),
        L"Enables the bundled movement-input fix DLC for faster transitions between movement states.");

    AddTooltip(AddControl(L"BUTTON",
        L"Alternate Presentation Resize (Experimental; VD foveated rendering)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 562, 640, 26,
        IdAlternatePresentationResize),
        L"Available when Presentation Size is below 1.00 and Asymmetric Projection is off. It keeps the runtime FOV unchanged and letterboxes the reduced image, enabling Virtual Desktop foveated-rendering experiments.");
    AddTooltip(AddControl(L"BUTTON", L"Asymmetric Projection (Experimental)",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 594, 610, 26,
        IdNativeStereo),
        L"Improves image quality at zero performance cost by matching the projection to your headset. The improvement depends on the headset and may be minimal or negligible on some models. Because it is experimental, it may cause visual artifacts or duplicated shader effects. It works with both AER + AFW and Stereo.");
    AddTooltip(AddControl(L"BUTTON", L"Diagnostic Logging",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 626, 410, 26,
        IdDiagnosticLogging),
        L"Writes witcher3vr.log and enables bounded runtime diagnostics. Use it for troubleshooting because it may affect performance.");

    AddTooltip(AddControl(L"BUTTON", L"First Person", BS_GROUPBOX,
        20, 682, 680, 146),
        L"These controls apply while the F11 First Person view is active.");
    AddTooltip(AddControl(L"BUTTON", L"Gamepad Snap Turn + Head Follow",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 708, 420, 28,
        IdFirstPersonGamepadHeadFollow),
        L"Turns the body in fixed gamepad steps and makes movement follow the headset direction while First Person is active.");
    AddTooltips(
        L"Select the number of degrees applied by each First Person gamepad snap turn.",
        {AddLabel(L"Angle", 475, 712, 60, 22),
         AddCombo(540, 704, 135, IdFirstPersonSnapTurnDegrees)});
    AddTooltip(AddControl(L"BUTTON", L"Auto switch to third person during combats",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 740, 630, 28,
        IdFirstPersonCombatExit),
        L"Leaves First Person when combat begins and returns after combat has safely ended. Manual view changes cancel the pending automatic return.");
    AddTooltip(AddControl(L"BUTTON", L"Strafe Movement",
        BS_AUTOCHECKBOX | WS_TABSTOP, 38, 772, 310, 26,
        IdFirstPersonStrafe),
        L"Keeps lateral input as strafing instead of turning Geralt while First Person is active.");
    AddTooltip(AddControl(L"BUTTON", L"Reduce Head Bobbing",
        BS_AUTOCHECKBOX | WS_TABSTOP, 365, 772, 310, 26,
        IdFirstPersonAnchorSmoothing),
        L"Smooths lateral and vertical First Person camera-anchor motion while preserving deliberate view rotation.");

    AddTooltip(AddControl(L"BUTTON", L"Bindings", BS_GROUPBOX,
        20, 842, 680, 112),
        L"Keyboard shortcuts available while Witcher 3 VR is running.");
    AddTooltip(AddLabel(
        L"F8  Standard / Near    F9  Recenter    F10  Cinema    F11  First Person (Experimental)",
        38, 868, 650, 24),
        L"Runtime view shortcuts: cycle the standard/near camera, recenter the headset, toggle Cinema3D, or toggle First Person.");
    AddTooltip(AddLabel(
        L"HUD editor: INS open / save and close    Q/E select panel    Arrow keys move    Wheel scales",
        38, 892, 650, 24),
        L"HUD editor controls: open or save with Insert, select a panel with Q/E, move it with the arrow keys, and resize it with the mouse wheel.");
    AddTooltip(AddLabel(
        L"R reset panel    X reset profile    F7 switch VR / Cinema3D (editor open or closed)",
        38, 916, 650, 24),
        L"HUD editor reset and preview controls: reset the current panel, reset the profile, or switch between Full VR and Cinema3D.");

    AddTooltip(AddLabel(L"", 20, 962, 680, 26, IdStatus, SS_LEFT),
        L"Shows validation results, saved changes, and launch status.");
    AddTooltip(AddControl(L"BUTTON", L"Configure Settings for VR",
        BS_PUSHBUTTON | WS_TABSTOP, 20, 992, 220, 34, IdConfigureVr),
        L"Installs the complete recommended VR graphics baseline, then reapplies the selected render mode and resolution.");
    AddTooltip(AddControl(L"BUTTON", L"Restore Original Settings",
        BS_PUSHBUTTON | WS_TABSTOP, 252, 992, 220, 34, IdRestoreOriginal),
        L"Restores the original dx12user.settings backup created by Configure Settings for VR.");
    AddTooltip(AddControl(L"BUTTON", L"Restore Defaults",
        BS_PUSHBUTTON | WS_TABSTOP, 484, 992, 216, 34, IdRestoreDefaults),
        L"Loads Witcher 3 VR launcher defaults into the controls. Press Save to apply them.");
    AddTooltip(AddControl(L"BUTTON", L"Save Only",
        BS_PUSHBUTTON | WS_TABSTOP, 406, 1038, 130, 36, IdSave),
        L"Writes the selected launcher, renderer, and game settings without starting the game.");
    AddTooltip(AddControl(L"BUTTON", L"Save && Launch",
        BS_DEFPUSHBUTTON | WS_TABSTOP, 550, 1038, 150, 36, IdSaveLaunch),
        L"Writes all settings, enforces render-mode compatibility, and starts The Witcher 3.");

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
        if (id == IdNativeStereo && notification == BN_CLICKED) {
            UpdateModeControls();
        }
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
        UpdateModeControls();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_DESTROY:
        if (g_app.tooltip && IsWindow(g_app.tooltip)) {
            DestroyWindow(g_app.tooltip);
            g_app.tooltip = nullptr;
        }
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

    EnsureHudEditorReady(nullptr);

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
