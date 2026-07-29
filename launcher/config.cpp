#include "config.h"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace w3vr {
namespace {

constexpr std::array<ModeSettings, 6> kModes{{
    {2, false, "none", 0, false},
    {2, false, "taau", 3, false},
    {2, false, "dlss", 6, true},
    {3, true, "none", 0, false},
    {3, true, "taau", 3, false},
    {3, true, "dlss", 6, true},
}};

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string Unquote(std::string value) {
    value = Trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

int ReadInt(const IniDocument& doc, const char* section, const char* key,
    int fallback) {
    const auto raw = doc.Get(section, key);
    if (!raw) return fallback;
    const auto value = Unquote(*raw);
    int result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} ? result : fallback;
}

float ReadFloat(const IniDocument& doc, const char* section, const char* key,
    float fallback) {
    const auto raw = doc.Get(section, key);
    if (!raw) return fallback;
    try {
        size_t consumed{};
        const float result = std::stof(Unquote(*raw), &consumed);
        return consumed > 0 && std::isfinite(result) ? result : fallback;
    } catch (...) {
        return fallback;
    }
}

bool ReadBool(const IniDocument& doc, const char* section, const char* key,
    bool fallback) {
    const auto raw = doc.Get(section, key);
    if (!raw) return fallback;
    const auto value = Lower(Unquote(*raw));
    if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off") return false;
    return fallback;
}

std::string ReadString(const IniDocument& doc, const char* section,
    const char* key, const char* fallback) {
    const auto raw = doc.Get(section, key);
    return raw ? Lower(Unquote(*raw)) : fallback;
}

std::string FloatString(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::wstring LastErrorMessage(const wchar_t* operation,
    const std::filesystem::path& path) {
    const DWORD code = GetLastError();
    wchar_t* system_message{};
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&system_message), 0, nullptr);
    std::wstring message = operation;
    message += L" failed for\n" + path.wstring() + L"\n\n";
    if (system_message) {
        message += system_message;
        LocalFree(system_message);
    } else {
        message += L"Windows error " + std::to_wstring(code);
    }
    return message;
}

std::optional<RenderMode> ExactMode(int xr_mode, bool dual_probe,
    bool dual_start, const std::string& backend, int aa_mode, bool allow_dlss) {
    for (size_t i = 0; i < kModes.size(); ++i) {
        const auto& mode = kModes[i];
        if (mode.openxr_mode == xr_mode && mode.dual_render == dual_probe &&
            mode.dual_render == dual_start && mode.temporal_backend == backend &&
            mode.aa_mode == aa_mode && mode.allow_dlss == allow_dlss) {
            return static_cast<RenderMode>(i);
        }
    }
    return std::nullopt;
}

RenderMode BestEffortMode(int xr_mode, const std::string& backend) {
    const bool stereo = xr_mode == 3 || xr_mode == 4;
    if (backend == "taau") return stereo ? RenderMode::StereoTaau : RenderMode::MonoTaau;
    if (backend == "dlss_packed") return RenderMode::StereoDlssSequential;
    if (backend == "dlss") return stereo ? RenderMode::StereoDlssSequential : RenderMode::MonoDlss;
    return stereo ? RenderMode::StereoNone : RenderMode::MonoNone;
}

} // namespace

IniDocument IniDocument::FromText(std::string text) {
    IniDocument document;
    size_t cursor{};
    while (cursor < text.size()) {
        const size_t end = text.find_first_of("\r\n", cursor);
        if (end == std::string::npos) {
            document.lines_.push_back({text.substr(cursor), {}});
            cursor = text.size();
            break;
        }
        size_t next = end + 1;
        std::string newline(1, text[end]);
        if (text[end] == '\r' && next < text.size() && text[next] == '\n') {
            newline = "\r\n";
            ++next;
        }
        if (document.lines_.empty()) document.default_newline_ = newline;
        document.lines_.push_back({text.substr(cursor, end - cursor), newline});
        cursor = next;
    }
    if (text.empty()) document.lines_.push_back({{}, {}});
    return document;
}

std::optional<IniDocument> IniDocument::Load(
    const std::filesystem::path& path, std::wstring& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = L"Could not open:\n" + path.wstring();
        return std::nullopt;
    }
    std::string bytes((std::istreambuf_iterator<char>(stream)), {});
    if (!stream.good() && !stream.eof()) {
        error = L"Could not read:\n" + path.wstring();
        return std::nullopt;
    }
    return FromText(std::move(bytes));
}

std::optional<std::string> IniDocument::Get(
    const std::string& wanted_section, const std::string& wanted_key) const {
    std::string section;
    for (const auto& line : lines_) {
        const std::string trimmed = Trim(line.text);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            section = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            continue;
        }
        if (section != Lower(wanted_section) || trimmed.empty() ||
            trimmed.front() == ';' || trimmed.front() == '#') continue;
        const size_t equals = line.text.find('=');
        if (equals == std::string::npos) continue;
        if (Lower(Trim(line.text.substr(0, equals))) == Lower(wanted_key)) {
            return Trim(line.text.substr(equals + 1));
        }
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> IniDocument::Entries(
    const std::string& wanted_section) const {
    std::vector<std::pair<std::string, std::string>> result;
    std::string section;
    for (const auto& line : lines_) {
        const std::string trimmed = Trim(line.text);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            section = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            continue;
        }
        if (section != Lower(wanted_section) || trimmed.empty() ||
            trimmed.front() == ';' || trimmed.front() == '#') continue;
        const size_t equals = line.text.find('=');
        if (equals == std::string::npos) continue;
        result.emplace_back(Trim(line.text.substr(0, equals)),
            Trim(line.text.substr(equals + 1)));
    }
    return result;
}

void IniDocument::Set(const std::string& wanted_section,
    const std::string& wanted_key, const std::string& value) {
    const std::string section_key = Lower(wanted_section);
    size_t section_line = lines_.size();
    size_t next_section = lines_.size();
    std::string section;
    for (size_t i = 0; i < lines_.size(); ++i) {
        const std::string trimmed = Trim(lines_[i].text);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            const std::string found = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            if (section_line != lines_.size() && next_section == lines_.size()) next_section = i;
            section = found;
            if (found == section_key) section_line = i;
            continue;
        }
        if (section != section_key || trimmed.empty() || trimmed.front() == ';' ||
            trimmed.front() == '#') continue;
        const size_t equals = lines_[i].text.find('=');
        if (equals != std::string::npos &&
            Lower(Trim(lines_[i].text.substr(0, equals))) == Lower(wanted_key)) {
            lines_[i].text = lines_[i].text.substr(0, equals + 1) + value;
            return;
        }
    }

    if (section_line == lines_.size()) {
        if (!lines_.empty() && lines_.back().newline.empty()) {
            lines_.back().newline = default_newline_;
        }
        if (!lines_.empty() && !lines_.back().text.empty()) {
            lines_.push_back({{}, default_newline_});
        }
        lines_.push_back({"[" + wanted_section + "]", default_newline_});
        lines_.push_back({wanted_key + "=" + value, {}});
        return;
    }

    if (next_section == lines_.size()) next_section = lines_.size();
    if (next_section > 0 && lines_[next_section - 1].newline.empty()) {
        lines_[next_section - 1].newline = default_newline_;
    }
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(next_section),
        {wanted_key + "=" + value, default_newline_});
}

std::string IniDocument::Serialize() const {
    std::string result;
    for (const auto& line : lines_) result += line.text + line.newline;
    return result;
}

const ModeSettings& SettingsForMode(RenderMode mode) {
    return kModes.at(static_cast<size_t>(mode));
}

const wchar_t* ModeDisplayName(RenderMode mode) {
    constexpr const wchar_t* names[]{
        L"Mono - No AA / FXAA", L"Mono - TAAU", L"Mono - DLSS",
        L"Stereo - No AA / FXAA", L"Stereo - TAAU",
        L"Stereo - DLSS Sequential"};
    return names[static_cast<size_t>(mode)];
}

bool ModeUsesDlss(RenderMode mode) {
    return mode == RenderMode::MonoDlss ||
        mode == RenderMode::StereoDlssSequential;
}

ConfigPaths DiscoverPaths() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
        static_cast<DWORD>(buffer.size()));
    std::filesystem::path directory = length > 0
        ? std::filesystem::path(std::wstring(buffer.data(), length)).parent_path()
        : std::filesystem::current_path();

    PWSTR documents_raw{};
    std::filesystem::path documents;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,
            KF_FLAG_DEFAULT, nullptr, &documents_raw))) {
        documents = documents_raw;
        CoTaskMemFree(documents_raw);
    }
    return {
        directory,
        directory / L"witcher3vr.ini",
        documents / L"The Witcher 3" / L"dx12user.settings",
        directory / L"witcher3.exe",
    };
}

bool EnsureVrConfiguration(const ConfigPaths& paths,
    const std::string& template_contents, bool& created, std::wstring& error) {
    created = false;
    const DWORD attributes = GetFileAttributesW(paths.vr_ini.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    const DWORD lookup_error = GetLastError();
    if (lookup_error != ERROR_FILE_NOT_FOUND &&
        lookup_error != ERROR_PATH_NOT_FOUND) {
        SetLastError(lookup_error);
        error = LastErrorMessage(L"Checking configuration", paths.vr_ini);
        return false;
    }

    // First-run AA inheritance: keep the user's supported native AA choice,
    // but always initialize its stereo Mode 3 route. Unknown/unsupported AA
    // values deliberately fall back to the safest release default, No AA.
    auto first_run = IniDocument::FromText(template_contents);
    std::wstring game_error;
    if (const auto game = IniDocument::Load(paths.game_settings, game_error)) {
        const int aa_mode = ReadInt(*game, "PostProcess", "AAMode", 0);
        const bool allow_dlss =
            ReadBool(*game, "Rendering", "AllowDLSS", false);
        const char* backend = "none";
        if (aa_mode == 3) {
            backend = "taau";
        } else if (aa_mode == 6 && allow_dlss) {
            backend = "dlss";
        }
        first_run.Set("openxr", "mode", "3");
        first_run.Set("engine", "dual_render_probe", "1");
        first_run.Set("engine", "dual_render_start", "1");
        first_run.Set("engine", "temporal_backend", backend);
    }
    if (!AtomicWriteWithBackup(
            paths.vr_ini, first_run.Serialize(), error)) {
        return false;
    }
    created = true;
    return true;
}

LoadResult LoadConfiguration(const ConfigPaths& paths) {
    LoadResult result;
    std::wstring error;
    const auto vr = IniDocument::Load(paths.vr_ini, error);
    if (!vr) {
        result.warning = error;
        return result;
    }
    const auto game = IniDocument::Load(paths.game_settings, error);
    if (!game) {
        result.warning = error;
        return result;
    }

    const int xr_mode = ReadInt(*vr, "openxr", "mode", 3);
    const bool dual_probe = ReadBool(*vr, "engine", "dual_render_probe", true);
    const bool dual_start = ReadBool(*vr, "engine", "dual_render_start", true);
    const auto backend = ReadString(*vr, "engine", "temporal_backend", "none");
    const bool dlss_dlaa = ReadBool(*vr, "engine", "dlss_dlaa", false);
    const int aa_mode = ReadInt(*game, "PostProcess", "AAMode", 0);
    const bool allow_dlss = ReadBool(*game, "Rendering", "AllowDLSS", false);
    if (const auto exact = ExactMode(xr_mode, dual_probe, dual_start, backend,
            aa_mode, allow_dlss)) {
        result.state.mode = *exact;
    } else {
        result.state.mode = BestEffortMode(xr_mode, backend);
        result.warning = L"The current AA/stereo settings are inconsistent. "
            L"The closest mode is displayed; no files are changed until Save.";
    }

    result.state.width = ReadInt(*vr, "openxr", "render_width", 2688);
    result.state.height = ReadInt(*vr, "openxr", "render_height", 2784);
    const int saved_dlss_quality = std::clamp(
        ReadInt(*game, "PostProcess", "DLSSQuality", 1), 0, 4);
    // Combo index 0 is the launcher-owned DLAA mode. NGX still receives the
    // supported Quality preset (1) as bootstrap; dlss_dlaa owns the distinction.
    result.state.dlss_quality =
        dlss_dlaa && ModeUsesDlss(result.state.mode)
        ? 0
        : std::clamp(saved_dlss_quality, 1, 4);
    result.state.hud_convergence_delta = std::clamp(
        ReadInt(*vr, "openxr", "hud_stereo_shift_px", -36) + 16, -64, 64);
    result.state.presentation_scale = std::clamp(
        ReadFloat(*vr, "openxr", "presentation_scale", 1.0f), 0.5f, 1.0f);
    result.state.hud_horizontal_scale = std::clamp(
        ReadFloat(*vr, "openxr", "hud_horizontal_scale", 0.5f), 0.25f, 1.0f);
    result.state.hud_vertical_scale = std::clamp(
        ReadFloat(*vr, "openxr", "hud_vertical_scale", 0.85f), 0.25f, 1.0f);
    result.state.menu_scale = std::clamp(
        ReadFloat(*vr, "openxr", "menu_scale", 0.85f), 0.3f, 1.5f);
    result.state.cinema_scale = std::clamp(
        ReadFloat(*vr, "openxr", "cinema_scale", result.state.menu_scale),
        0.3f, 1.5f);
    result.state.near_view = std::clamp(
        ReadFloat(*vr, "engine", "close_camera_offset", 0.75f), -2.0f, 3.0f);
    result.state.vertical_pitch_enabled = ReadBool(
        *vr, "openxr", "vertical_pitch_enabled", false);
    result.state.cinema_5x4 = ReadBool(
        *vr, "openxr", "cinema_5x4", true);
    result.state.steady_icons = ReadBool(
        *vr, "openxr", "steady_icons", false);
    result.state.first_person_gamepad_head_follow =
        ReadBool(*vr, "engine", "first_person_snap_turn", false) &&
        ReadBool(*vr, "engine", "first_person_hmd_body_follow", false);
    result.state.first_person_combat_exit = ReadBool(
        *vr, "engine", "first_person_combat_exit", false);
    result.state.diagnostic_logging =
        ReadBool(*vr, "debug", "logging_enabled", false) &&
        ReadBool(*vr, "debug", "runtime_diagnostics", false);
    return result;
}

CompatibilityWarnings InspectCompatibilitySettings(const ConfigPaths& paths) {
    CompatibilityWarnings warnings;
    std::wstring error;
    const auto game = IniDocument::Load(paths.game_settings, error);
    if (!game) {
        return warnings;
    }
    warnings.ray_tracing_enabled =
        ReadBool(*game, "Rendering/RT", "EnableRT", false);
    // In the DX12 settings file SSREnabled is the High-quality switch. False
    // covers the supported Low/Off choices.
    warnings.ssr_high =
        ReadBool(*game, "Rendering", "SSREnabled", false);
    return warnings;
}

bool BuildUpdatedDocuments(const ConfigPaths& paths, const LauncherState& state,
    IniDocument& vr_ini, IniDocument& game_settings, std::wstring& error) {
    const auto loaded_vr = IniDocument::Load(paths.vr_ini, error);
    if (!loaded_vr) return false;
    const auto loaded_game = IniDocument::Load(paths.game_settings, error);
    if (!loaded_game) return false;
    vr_ini = *loaded_vr;
    game_settings = *loaded_game;

    const auto& mode = SettingsForMode(state.mode);
    vr_ini.Set("openxr", "mode", std::to_string(mode.openxr_mode));
    vr_ini.Set("openxr", "render_width", std::to_string(state.width));
    vr_ini.Set("openxr", "render_height", std::to_string(state.height));
    vr_ini.Set("openxr", "hud_stereo_shift_px",
        std::to_string(std::clamp(state.hud_convergence_delta - 16, -256, 256)));
    vr_ini.Set("openxr", "presentation_scale", FloatString(state.presentation_scale));
    vr_ini.Set("openxr", "hud_horizontal_scale", FloatString(state.hud_horizontal_scale));
    vr_ini.Set("openxr", "hud_vertical_scale", FloatString(state.hud_vertical_scale));
    vr_ini.Set("openxr", "menu_scale", FloatString(state.menu_scale));
    vr_ini.Set("openxr", "cinema_scale", FloatString(state.cinema_scale));
    vr_ini.Set("openxr", "vertical_pitch_enabled",
        state.vertical_pitch_enabled ? "1" : "0");
    vr_ini.Set("openxr", "cinema_5x4", state.cinema_5x4 ? "1" : "0");
    vr_ini.Set("openxr", "steady_icons", state.steady_icons ? "1" : "0");
    vr_ini.Set("engine", "close_camera_offset", FloatString(state.near_view));
    vr_ini.Set("engine", "dual_render_probe", mode.dual_render ? "1" : "0");
    vr_ini.Set("engine", "dual_render_start", mode.dual_render ? "1" : "0");
    vr_ini.Set("engine", "temporal_backend", mode.temporal_backend);
    // One experimental launcher option owns both cooperating F11 controls.
    // Keeping them equal avoids a persistent snap preview without its
    // continuous native-camera follower, or the inverse partial state.
    vr_ini.Set("engine", "first_person_snap_turn",
        state.first_person_gamepad_head_follow ? "1" : "0");
    vr_ini.Set("engine", "first_person_hmd_body_follow",
        state.first_person_gamepad_head_follow ? "1" : "0");
    vr_ini.Set("engine", "first_person_combat_exit",
        state.first_person_combat_exit ? "1" : "0");
    const bool dlss_dlaa = ModeUsesDlss(state.mode) && state.dlss_quality == 0;
    vr_ini.Set("engine", "dlss_dlaa", dlss_dlaa ? "1" : "0");
    // [DEBUG 1/2] One launcher switch owns both the log writer and the
    // higher-detail runtime probes, so release and diagnostic runs use one DLL.
    vr_ini.Set("debug", "logging_enabled",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "runtime_diagnostics",
        state.diagnostic_logging ? "1" : "0");

    game_settings.Set("Viewport", "Resolution", "\"" +
        std::to_string(state.width) + "x" + std::to_string(state.height) + "\"");
    game_settings.Set("PostProcess", "AAMode", std::to_string(mode.aa_mode));
    game_settings.Set("PostProcess", "DLSSQuality",
        std::to_string(state.dlss_quality == 0
            ? 1
            : std::clamp(state.dlss_quality, 1, 4)));
    game_settings.Set("Rendering", "AllowDLSS",
        mode.allow_dlss ? "true" : "false");
    return true;
}

bool AtomicWriteWithBackup(const std::filesystem::path& path,
    const std::string& contents, std::wstring& error) {
    const auto temporary = path.wstring() + L".w3vr.tmp";
    const auto backup = path.wstring() + L".w3vr.bak";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = LastErrorMessage(L"Creating temporary file", temporary);
        return false;
    }
    DWORD written{};
    const bool write_ok = contents.size() <= MAXDWORD &&
        WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()),
            &written, nullptr) && written == contents.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!write_ok) {
        error = LastErrorMessage(L"Writing temporary file", temporary);
        DeleteFileW(temporary.c_str());
        return false;
    }

    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(backup.c_str());
        if (!ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(),
                REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            error = LastErrorMessage(L"Replacing configuration", path);
            DeleteFileW(temporary.c_str());
            return false;
        }
    } else if (!MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING)) {
        error = LastErrorMessage(L"Installing configuration", path);
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

bool SaveConfiguration(const ConfigPaths& paths, const LauncherState& state,
    std::wstring& error) {
    IniDocument vr;
    IniDocument game;
    if (!BuildUpdatedDocuments(paths, state, vr, game, error)) return false;
    if (!AtomicWriteWithBackup(paths.vr_ini, vr.Serialize(), error)) return false;
    if (!AtomicWriteWithBackup(paths.game_settings, game.Serialize(), error)) return false;
    return true;
}

std::filesystem::path OriginalSettingsBackupPath(const ConfigPaths& paths) {
    return paths.game_settings.wstring() + L".w3vr-original.bak";
}

bool HasOriginalSettingsBackup(const ConfigPaths& paths) {
    return GetFileAttributesW(OriginalSettingsBackupPath(paths).c_str()) !=
        INVALID_FILE_ATTRIBUTES;
}

bool ConfigureGameSettingsForVr(const ConfigPaths& paths,
    const std::string& template_contents, std::wstring& error) {
    const auto current = IniDocument::Load(paths.game_settings, error);
    if (!current) return false;

    auto configured = IniDocument::FromText(template_contents);
    // The bundled profile intentionally contains no user credentials. Preserve
    // the complete target Galaxy section so configuring graphics never signs a
    // user out or copies the developer's account data to another installation.
    for (const auto& [key, value] : current->Entries("Galaxy")) {
        configured.Set("Galaxy", key, value);
    }

    const auto backup = OriginalSettingsBackupPath(paths);
    if (!HasOriginalSettingsBackup(paths) &&
        !CopyFileW(paths.game_settings.c_str(), backup.c_str(), TRUE)) {
        error = LastErrorMessage(L"Creating original settings backup", backup);
        return false;
    }
    return AtomicWriteWithBackup(paths.game_settings, configured.Serialize(), error);
}

bool RestoreOriginalGameSettings(const ConfigPaths& paths, std::wstring& error) {
    const auto backup = OriginalSettingsBackupPath(paths);
    std::ifstream stream(backup, std::ios::binary);
    if (!stream) {
        error = L"Original settings backup was not found:\n" + backup.wstring();
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), {});
    if (!stream.good() && !stream.eof()) {
        error = L"Could not read original settings backup:\n" + backup.wstring();
        return false;
    }
    stream.close();
    if (!AtomicWriteWithBackup(paths.game_settings, contents, error)) return false;
    if (!DeleteFileW(backup.c_str())) {
        error = LastErrorMessage(L"Removing restored settings backup", backup);
        return false;
    }
    return true;
}

} // namespace w3vr
