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
#include <string_view>

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

constexpr int kCurrentConfigVersion = 6;
constexpr float kCinemaHudReferenceScale = 1.30f;
constexpr int kCinemaHudReferenceShift = -72;
constexpr float kFullVrHudReferenceScale = 1.00f;
// The V1022 physical HUD plane makes shift * size the inverse-depth authority.
// Full VR now defaults directly to gameplay's validated -36 at size 1.0:
// about one metre on the Quest 3 reference instead of the old ~25 cm plane.
constexpr int kFullVrHudReferenceShift = -36;

int ProportionalHudConvergenceShift(
    float hud_scale,
    float reference_scale,
    int reference_shift,
    int offset) {
    // Convergence is authored in source HUD pixels. Scale its validated base
    // by the selected HUD-size ratio first; the user-facing slider is then a
    // small correction around that automatic value, never the whole value.
    const float safe_scale = std::clamp(hud_scale, 0.5f, 1.5f);
    const int automatic_shift = static_cast<int>(std::lround(
        static_cast<float>(reference_shift) * safe_scale /
        reference_scale));
    return std::clamp(automatic_shift + std::clamp(offset, -64, 64),
        -512, 512);
}

int PhysicalPlaneHudConvergenceShift(
    float hud_scale,
    float reference_scale,
    int reference_shift,
    int offset) {
    // A physical plane derives inverse distance from source shift * HUD size.
    // Vary shift inversely with size so the size control does not also move
    // the plane toward or away from the viewer.
    const float safe_scale = std::clamp(hud_scale, 0.5f, 1.5f);
    const int automatic_shift = static_cast<int>(std::lround(
        static_cast<float>(reference_shift) * reference_scale /
        safe_scale));
    return std::clamp(automatic_shift + std::clamp(offset, -64, 64),
        -512, 512);
}

int ReadInt(const IniDocument& doc, const char* section, const char* key,
    int fallback);
float ReadFloat(const IniDocument& doc, const char* section, const char* key,
    float fallback);

void RemoveObsoleteSettings(IniDocument& ini) {
    ini.Remove("openxr", "snap_turn_enabled");
    ini.Remove("openxr", "snap_turn_angle");
    ini.Remove("openxr", "hud_convergence_offset_px");
    ini.Remove("openxr", "cinematic_16_9");
    ini.Remove("openxr", "cinema_subtitle_scale");
    ini.Remove("openxr", "full_vr_subtitle_scale");
    ini.Remove("openxr", "cinema_subtitle_stereo_shift_px");
    ini.Remove("engine", "streamline_ps93_learning_log");
}

void MigrateConfigurationToV2(IniDocument& ini) {
    // These are the validated 0.9.1 defaults. Apply them once to INIs from the
    // first alpha, while leaving render mode, resolution and user-owned values
    // untouched. The version marker prevents later launcher starts from
    // overwriting manual tuning.
    ini.Set("openxr", "cinema_render_stereo_strength", "0.250");
    ini.Set("openxr", "cinema_hud_stereo_shift_px", "-72");
    ini.Set("openxr", "manual_cinema_hud_scale", "1.600");
    ini.Set("openxr", "full_vr_hud_stereo_shift_px", "-192");
    ini.Set("openxr", "full_vr_hud_scale", "0.750");
    ini.Set("openxr", "cinema_5x4", "1");
    if (!ini.Get("openxr", "cinema_full_vr").has_value()) {
        ini.Set("openxr", "cinema_full_vr", "1");
    }
    ini.Set("reverse", "enabled", "0");
    ini.Set("debug", "logging_enabled", "0");
    ini.Set("debug", "runtime_diagnostics", "0");
    ini.Set("debug", "taau_drop_diagnostics", "0");
    ini.Set("debug", "cinema_camera_diagnostics", "0");
    ini.Set("debug", "cinema_subtitle_diagnostics", "0");
    ini.Set("debug", "first_person_state_diagnostics", "0");
    ini.Set("debug", "first_person_aim_diagnostics", "0");
    ini.Set("debug", "world_marker_diagnostics", "0");
    RemoveObsoleteSettings(ini);
    ini.Set("meta", "config_version", "2");
}

void MigrateConfigurationToV3(IniDocument& ini) {
    // V3 exposes the automatic Cinema3D HUD scale and makes the stationary
    // first-person body turn optional. Seed only absent values so a V2 user's
    // advanced INI tuning remains authoritative.
    if (!ini.Get("openxr", "cinema_hud_scale").has_value()) {
        ini.Set("openxr", "cinema_hud_scale", "1.300");
    }
    if (!ini.Get("engine", "first_person_stationary_turn").has_value()) {
        ini.Set("engine", "first_person_stationary_turn", "0");
    }
    RemoveObsoleteSettings(ini);
    ini.Set("meta", "config_version", "3");
}

void MigrateConfigurationToV4(IniDocument& ini) {
    // The independent X/Y HUD zoom controls were removed from the supported
    // launcher surface in V978. Normalize values inherited from older INIs
    // once so the compositor starts from an undistorted 1:1 HUD again.
    ini.Set("openxr", "hud_horizontal_scale", "1.000");
    ini.Set("openxr", "hud_vertical_scale", "1.000");
    RemoveObsoleteSettings(ini);
    ini.Set("meta", "config_version", "4");
}

void MigrateConfigurationToV5(IniDocument& ini) {
    // Preserve the user's old slider offset while moving Full VR onto the
    // physical-plane depth contract. V4 stored a proportional -192/0.75 base;
    // V5 stores an inverse-size -36/1.0 base at gameplay-HUD depth.
    const float previous_hud_scale = std::clamp(
        ReadFloat(ini, "openxr", "full_vr_hud_scale", 0.75f),
        0.5f, 1.5f);
    const int stored_shift = ReadInt(
        ini, "openxr", "full_vr_hud_stereo_shift_px", -192);
    const int old_automatic_shift = static_cast<int>(std::lround(
        -192.0f * previous_hud_scale / 0.75f));
    const int preserved_offset = std::clamp(
        stored_shift - old_automatic_shift, -64, 64);
    ini.Set("openxr", "full_vr_hud_scale", "1.000");
    ini.Set("openxr", "full_vr_hud_stereo_shift_px", std::to_string(
        PhysicalPlaneHudConvergenceShift(
            1.0f, kFullVrHudReferenceScale,
            kFullVrHudReferenceShift, preserved_offset)));
    RemoveObsoleteSettings(ini);
    ini.Set("meta", "config_version", "5");
}

void MigrateConfigurationToV6(IniDocument& ini) {
    // Keep the automatic shader registry independent from the launcher's broad
    // Diagnostic Logging switch. Existing manual values remain authoritative.
    if (!ini.Get("focus_projection", "shader_registry_enabled").has_value()) {
        ini.Set("focus_projection", "shader_registry_enabled", "0");
    }
    RemoveObsoleteSettings(ini);
    ini.Set("meta", "config_version", std::to_string(kCurrentConfigVersion));
}

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

struct EditableTextLine {
    std::string text;
    std::string newline;
};

std::vector<EditableTextLine> ParseEditableText(const std::string& text) {
    std::vector<EditableTextLine> lines;
    size_t cursor{};
    while (cursor < text.size()) {
        const size_t end = text.find_first_of("\r\n", cursor);
        if (end == std::string::npos) {
            lines.push_back({text.substr(cursor), {}});
            break;
        }
        size_t next = end + 1;
        std::string newline(1, text[end]);
        if (text[end] == '\r' && next < text.size() && text[next] == '\n') {
            newline = "\r\n";
            ++next;
        }
        lines.push_back({text.substr(cursor, end - cursor), newline});
        cursor = next;
    }
    return lines;
}

std::string SerializeEditableText(
    const std::vector<EditableTextLine>& lines) {
    std::string result;
    for (const auto& line : lines) result += line.text + line.newline;
    return result;
}

std::string PreferredNewline(const std::vector<EditableTextLine>& lines) {
    for (const auto& line : lines) {
        if (!line.newline.empty()) return line.newline;
    }
    return "\r\n";
}

constexpr std::string_view kHudEditorFilelistRegistration =
    "modWitcher3VRHUDEditor.xml;";
constexpr std::u16string_view kHudEditorFilelistRegistrationUtf16 =
    u"modWitcher3VRHUDEditor.xml;";

bool Utf16LineIsHudEditorRegistration(std::u16string_view line) {
    const auto is_space = [](char16_t value) {
        return value == u' ' || value == u'\t' || value == u'\r' ||
            value == u'\n';
    };
    while (!line.empty() && is_space(line.front())) line.remove_prefix(1);
    while (!line.empty() && is_space(line.back())) line.remove_suffix(1);
    return line == kHudEditorFilelistRegistrationUtf16;
}

bool Utf16HasHudEditorRegistration(std::u16string_view text) {
    size_t cursor{};
    while (cursor <= text.size()) {
        size_t end = cursor;
        while (end < text.size() && text[end] != u'\r' &&
            text[end] != u'\n') {
            ++end;
        }
        if (Utf16LineIsHudEditorRegistration(
                text.substr(cursor, end - cursor))) {
            return true;
        }
        if (end == text.size()) break;
        cursor = end + 1;
        if (text[end] == u'\r' && cursor < text.size() &&
            text[cursor] == u'\n') {
            ++cursor;
        }
    }
    return false;
}

std::u16string PreferredUtf16Newline(std::u16string_view text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\r') {
            return i + 1 < text.size() && text[i + 1] == u'\n'
                ? u"\r\n" : u"\r";
        }
        if (text[i] == u'\n') return u"\n";
    }
    return u"\r\n";
}

bool IsMixedEncodingHudEditorSuffix(
    const std::string& bytes, size_t registration_position) {
    size_t cursor = registration_position;
    while (cursor < bytes.size()) {
        if (bytes.compare(cursor, kHudEditorFilelistRegistration.size(),
                kHudEditorFilelistRegistration) == 0) {
            cursor += kHudEditorFilelistRegistration.size();
            continue;
        }
        if (bytes[cursor] == '\r' || bytes[cursor] == '\n') {
            ++cursor;
            continue;
        }
        return false;
    }
    return true;
}

std::optional<std::string> PrepareHudEditorFilelist(
    const std::string& original) {
    const bool utf16_le = original.size() >= 2 &&
        static_cast<unsigned char>(original[0]) == 0xFF &&
        static_cast<unsigned char>(original[1]) == 0xFE;
    const bool utf16_be = original.size() >= 2 &&
        static_cast<unsigned char>(original[0]) == 0xFE &&
        static_cast<unsigned char>(original[1]) == 0xFF;
    if (!utf16_le && !utf16_be) {
        auto lines = ParseEditableText(original);
        const bool registered = std::any_of(lines.begin(), lines.end(),
            [](const EditableTextLine& line) {
                return Trim(line.text) == kHudEditorFilelistRegistration;
            });
        if (registered) return original;

        const std::string newline = PreferredNewline(lines);
        if (!lines.empty() && lines.back().newline.empty()) {
            lines.back().newline = newline;
        }
        lines.push_back(
            {std::string(kHudEditorFilelistRegistration), newline});
        return SerializeEditableText(lines);
    }

    // Older launchers parsed the UTF-16 file as narrow text and appended an
    // ASCII registration to it. That produces the reported Chinese-looking
    // suffix in editors. Recognize only the exact launcher-owned suffix before
    // truncating it, then rebuild the registration in the original encoding.
    std::string clean = original;
    const size_t mixed_registration =
        clean.find(kHudEditorFilelistRegistration);
    if (mixed_registration != std::string::npos &&
        IsMixedEncodingHudEditorSuffix(clean, mixed_registration)) {
        size_t valid_end = mixed_registration;
        while (valid_end > 2 &&
            (clean[valid_end - 1] == '\r' || clean[valid_end - 1] == '\n')) {
            --valid_end;
        }
        if ((valid_end - 2) % 2 != 0) return std::nullopt;
        clean.resize(valid_end);
    }

    if (clean.size() < 2 || (clean.size() - 2) % 2 != 0) {
        return std::nullopt;
    }

    std::u16string text;
    text.reserve((clean.size() - 2) / 2);
    for (size_t i = 2; i < clean.size(); i += 2) {
        const auto first = static_cast<unsigned char>(clean[i]);
        const auto second = static_cast<unsigned char>(clean[i + 1]);
        text.push_back(static_cast<char16_t>(utf16_le
            ? first | (static_cast<unsigned int>(second) << 8)
            : (static_cast<unsigned int>(first) << 8) | second));
    }

    if (!Utf16HasHudEditorRegistration(text)) {
        const std::u16string newline = PreferredUtf16Newline(text);
        if (!text.empty() && text.back() != u'\r' && text.back() != u'\n') {
            text += newline;
        }
        text += kHudEditorFilelistRegistrationUtf16;
        text += newline;
    }

    std::string prepared;
    prepared.reserve(2 + text.size() * 2);
    prepared.push_back(utf16_le ? static_cast<char>(0xFF) :
        static_cast<char>(0xFE));
    prepared.push_back(utf16_le ? static_cast<char>(0xFE) :
        static_cast<char>(0xFF));
    for (const char16_t value : text) {
        const char low = static_cast<char>(value & 0xFF);
        const char high = static_cast<char>((value >> 8) & 0xFF);
        prepared.push_back(utf16_le ? low : high);
        prepared.push_back(utf16_le ? high : low);
    }
    return prepared;
}

bool SectionBounds(const std::vector<EditableTextLine>& lines,
    const std::string& wanted_section, size_t& header, size_t& end) {
    const std::string wanted = Lower(wanted_section);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i].text);
        if (trimmed.size() < 2 || trimmed.front() != '[' ||
            trimmed.back() != ']') {
            continue;
        }
        if (Lower(Trim(trimmed.substr(1, trimmed.size() - 2))) != wanted) {
            continue;
        }
        header = i;
        end = lines.size();
        for (size_t j = i + 1; j < lines.size(); ++j) {
            const std::string candidate = Trim(lines[j].text);
            if (candidate.size() >= 2 && candidate.front() == '[' &&
                candidate.back() == ']') {
                end = j;
                break;
            }
        }
        return true;
    }
    return false;
}

bool SectionHasAction(const std::vector<EditableTextLine>& lines,
    const std::string& section, const std::string& action) {
    size_t header{};
    size_t end{};
    if (!SectionBounds(lines, section, header, end)) return false;
    const std::string token = "Action=" + action;
    for (size_t i = header + 1; i < end; ++i) {
        if (lines[i].text.find(token) != std::string::npos) return true;
    }
    return false;
}

void EnsureSectionAction(std::vector<EditableTextLine>& lines,
    const std::string& section, const std::string& action,
    const std::string& default_binding, const std::string& newline) {
    if (SectionHasAction(lines, section, action)) return;

    size_t header{};
    size_t end{};
    if (!SectionBounds(lines, section, header, end)) {
        if (!lines.empty() && lines.back().newline.empty()) {
            lines.back().newline = newline;
        }
        if (!lines.empty() && !lines.back().text.empty()) {
            lines.push_back({{}, newline});
        }
        lines.push_back({"[" + section + "]", newline});
        lines.push_back({default_binding, newline});
        return;
    }

    size_t insertion = end;
    while (insertion > header + 1 && lines[insertion - 1].text.empty()) {
        --insertion;
    }
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertion),
        {default_binding, newline});
}

void RemoveLegacyHudEditorBindings(std::vector<EditableTextLine>& lines) {
    size_t header{};
    size_t end{};
    if (!SectionBounds(lines, "W3VRHudEditor", header, end)) return;

    const bool legacy_arrows =
        !SectionHasAction(lines, "W3VRHudEditor", "W3VRHudEditorMoveLeft") &&
        !SectionHasAction(lines, "W3VRHudEditor", "W3VRHudEditorMoveRight") &&
        std::any_of(lines.begin() + static_cast<std::ptrdiff_t>(header + 1),
            lines.begin() + static_cast<std::ptrdiff_t>(end),
            [](const EditableTextLine& line) {
                return Trim(line.text) ==
                    "IK_Left=(Action=W3VRHudEditorPrevious)";
            }) &&
        std::any_of(lines.begin() + static_cast<std::ptrdiff_t>(header + 1),
            lines.begin() + static_cast<std::ptrdiff_t>(end),
            [](const EditableTextLine& line) {
                return Trim(line.text) ==
                    "IK_Right=(Action=W3VRHudEditorNext)";
            });

    for (size_t i = end; i-- > header + 1;) {
        const std::string line = Trim(lines[i].text);
        const bool obsolete =
            line.find("Action=W3VRHudEditorMoveX") != std::string::npos ||
            line.find("Action=W3VRHudEditorMoveY") != std::string::npos ||
            line.find("Action=W3VRHudEditorDrag") != std::string::npos ||
            line.find("Action=W3VRHudEditorExit") != std::string::npos ||
            line.find("Action=W3VRHudEditorProfile") != std::string::npos ||
            line == "IK_MouseX=(Action=GI_MouseDampX)" ||
            line == "IK_MouseY=(Action=GI_MouseDampY)" ||
            (legacy_arrows &&
                (line == "IK_Left=(Action=W3VRHudEditorPrevious)" ||
                 line == "IK_Right=(Action=W3VRHudEditorNext)"));
        if (obsolete) {
            lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

std::string MergeHudEditorBindings(const std::string& original) {
    auto lines = ParseEditableText(original);
    const std::string newline = PreferredNewline(lines);
    RemoveLegacyHudEditorBindings(lines);

    constexpr std::array<const char*, 12> toggle_contexts{
        "Boat", "BoatPassenger", "Combat", "Combat_Replacer_Ciri",
        "Diving", "Exploration", "Exploration_Replacer_Ciri", "Horse",
        "Horse_Replacer_Ciri", "JumpClimb", "Scene", "Swimming"};
    for (const char* context : toggle_contexts) {
        EnsureSectionAction(lines, context, "W3VRHudEditorToggle",
            "IK_Insert=(Action=W3VRHudEditorToggle)", newline);
        EnsureSectionAction(lines, context, "W3VRHudEditorProfile",
            "IK_F7=(Action=W3VRHudEditorProfile)", newline);
    }

    constexpr std::array<std::pair<const char*, const char*>, 11> editor_actions{{
        {"W3VRHudEditorToggle", "IK_Insert=(Action=W3VRHudEditorToggle)"},
        {"W3VRHudEditorPrevious", "IK_Q=(Action=W3VRHudEditorPrevious)"},
        {"W3VRHudEditorNext", "IK_E=(Action=W3VRHudEditorNext)"},
        {"W3VRHudEditorMoveLeft", "IK_Left=(Action=W3VRHudEditorMoveLeft)"},
        {"W3VRHudEditorMoveRight", "IK_Right=(Action=W3VRHudEditorMoveRight)"},
        {"W3VRHudEditorMoveUp", "IK_Up=(Action=W3VRHudEditorMoveUp)"},
        {"W3VRHudEditorMoveDown", "IK_Down=(Action=W3VRHudEditorMoveDown)"},
        {"W3VRHudEditorScale", "IK_MouseZ=(Action=W3VRHudEditorScale)"},
        {"W3VRHudEditorResetCurrent", "IK_R=(Action=W3VRHudEditorResetCurrent)"},
        {"W3VRHudEditorResetProfile", "IK_X=(Action=W3VRHudEditorResetProfile)"},
        {"W3VRHudEditorProfile", "IK_F7=(Action=W3VRHudEditorProfile)"},
    }};
    for (const auto& [action, binding] : editor_actions) {
        EnsureSectionAction(lines, "W3VRHudEditor", action, binding, newline);
    }
    return SerializeEditableText(lines);
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

void IniDocument::Remove(const std::string& wanted_section,
    const std::string& wanted_key) {
    const std::string section_key = Lower(wanted_section);
    const std::string key = Lower(wanted_key);
    std::string section;
    for (auto line = lines_.begin(); line != lines_.end();) {
        const std::string trimmed = Trim(line->text);
        if (trimmed.size() >= 2 && trimmed.front() == '[' &&
            trimmed.back() == ']') {
            section = Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            ++line;
            continue;
        }
        const size_t equals = line->text.find('=');
        if (section == section_key && !trimmed.empty() &&
            trimmed.front() != ';' && trimmed.front() != '#' &&
            equals != std::string::npos &&
            Lower(Trim(line->text.substr(0, equals))) == key) {
            line = lines_.erase(line);
        } else {
            ++line;
        }
    }
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
        L"Stereo - DLSS"};
    return names[static_cast<size_t>(mode)];
}

bool ModeUsesDlss(RenderMode mode) {
    return mode == RenderMode::MonoDlss ||
        mode == RenderMode::StereoDlssSequential;
}

int CinemaHudConvergenceShift(float hud_scale, int offset) {
    return ProportionalHudConvergenceShift(
        hud_scale, kCinemaHudReferenceScale,
        kCinemaHudReferenceShift, offset);
}

int FullVrHudConvergenceShift(float hud_scale, int offset) {
    return PhysicalPlaneHudConvergenceShift(
        hud_scale, kFullVrHudReferenceScale,
        kFullVrHudReferenceShift, offset);
}

std::optional<int> DlssNearSquareCompatibleWidth(const LauncherState& state) {
    constexpr int kResolutionAdjustment = 48;
    constexpr int kMinimumResolution = 640;
    constexpr int kMaximumResolution = 8192;
    if (!ModeUsesDlss(state.mode) || state.dlss_quality == 0 ||
        std::abs(state.width - state.height) >= kResolutionAdjustment) {
        return std::nullopt;
    }
    const int adjusted_width = state.width <= state.height
        ? state.width - kResolutionAdjustment
        : state.width + kResolutionAdjustment;
    if (adjusted_width >= kMinimumResolution &&
        adjusted_width <= kMaximumResolution) {
        return adjusted_width;
    }
    return std::nullopt;
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

std::wstring HudEditorManualSetupInstructions(const ConfigPaths& paths) {
    const auto bin_directory = paths.launcher_directory.parent_path();
    const auto filelist = bin_directory / L"config" / L"r4game" /
        L"user_config_matrix" / L"pc" / L"dx12filelist.txt";
    const auto input_settings =
        paths.game_settings.parent_path() / L"input.settings";

    std::wostringstream guide;
    guide << L"Witcher 3 VR HUD Editor - manual setup\r\n\r\n"
        L"Close The Witcher 3 completely before editing these files. The game "
        L"can overwrite input.settings when it exits.\r\n\r\n"
        L"1. Open:\r\n" << filelist.wstring() << L"\r\n\r\n"
        L"Add this line once:\r\n"
        L"modWitcher3VRHUDEditor.xml;\r\n\r\n"
        L"2. Open:\r\n" << input_settings.wstring() << L"\r\n\r\n"
        L"Under each of these existing sections:\r\n"
        L"Boat, BoatPassenger, Combat, Combat_Replacer_Ciri, Diving, "
        L"Exploration, Exploration_Replacer_Ciri, Horse, "
        L"Horse_Replacer_Ciri, JumpClimb, Scene, Swimming\r\n\r\n"
        L"add these two lines once:\r\n"
        L"IK_Insert=(Action=W3VRHudEditorToggle)\r\n"
        L"IK_F7=(Action=W3VRHudEditorProfile)\r\n\r\n"
        L"Add this section, or add the missing lines to the existing section:\r\n"
        L"[W3VRHudEditor]\r\n"
        L"IK_Insert=(Action=W3VRHudEditorToggle)\r\n"
        L"IK_F7=(Action=W3VRHudEditorProfile)\r\n"
        L"IK_Q=(Action=W3VRHudEditorPrevious)\r\n"
        L"IK_E=(Action=W3VRHudEditorNext)\r\n"
        L"IK_Left=(Action=W3VRHudEditorMoveLeft)\r\n"
        L"IK_Right=(Action=W3VRHudEditorMoveRight)\r\n"
        L"IK_Up=(Action=W3VRHudEditorMoveUp)\r\n"
        L"IK_Down=(Action=W3VRHudEditorMoveDown)\r\n"
        L"IK_MouseZ=(Action=W3VRHudEditorScale)\r\n"
        L"IK_R=(Action=W3VRHudEditorResetCurrent)\r\n"
        L"IK_X=(Action=W3VRHudEditorResetProfile)\r\n\r\n"
        L"Save both files, then start the game through the launcher.\r\n";
    return guide.str();
}

bool EnsureVrConfiguration(const ConfigPaths& paths,
    const std::string& template_contents, bool& created, std::wstring& error) {
    created = false;
    const DWORD attributes = GetFileAttributesW(paths.vr_ini.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        const auto existing = IniDocument::Load(paths.vr_ini, error);
        if (!existing) return false;
        const int existing_version = ReadInt(
            *existing, "meta", "config_version", 0);
        if (existing_version >= kCurrentConfigVersion) {
            return true;
        }
        auto migrated = *existing;
        if (existing_version < 2) {
            MigrateConfigurationToV2(migrated);
        }
        if (existing_version < 3) {
            MigrateConfigurationToV3(migrated);
        }
        if (existing_version < 4) {
            MigrateConfigurationToV4(migrated);
        }
        if (existing_version < 5) {
            MigrateConfigurationToV5(migrated);
        }
        if (existing_version < 6) {
            MigrateConfigurationToV6(migrated);
        }
        return AtomicWriteWithBackup(paths.vr_ini, migrated.Serialize(), error);
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

bool EnsureHudEditorSetup(const ConfigPaths& paths, std::wstring& error) {
    const auto bin_directory = paths.launcher_directory.parent_path();
    const auto game_root = bin_directory.parent_path();
    const auto script = game_root / L"mods" / L"modWitcher3VRHUDEditor" /
        L"content" / L"scripts" / L"local" /
        L"witcher3vr_hud_editor" / L"hud_editor.ws";
    const auto config_directory = bin_directory / L"config" / L"r4game" /
        L"user_config_matrix" / L"pc";
    const auto xml = config_directory / L"modWitcher3VRHUDEditor.xml";
    const auto filelist = config_directory / L"dx12filelist.txt";
    const auto input_settings =
        paths.game_settings.parent_path() / L"input.settings";

    for (const auto& required : {script, xml, filelist, input_settings}) {
        const DWORD attributes = GetFileAttributesW(required.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            error = L"HUD editor setup file was not found:\n" +
                required.wstring();
            return false;
        }
    }

    const auto read_text = [&error](const std::filesystem::path& path)
        -> std::optional<std::string> {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            error = L"Could not open HUD setup file:\n" + path.wstring();
            return std::nullopt;
        }
        std::string contents((std::istreambuf_iterator<char>(stream)), {});
        if (!stream.good() && !stream.eof()) {
            error = L"Could not read HUD setup file:\n" + path.wstring();
            return std::nullopt;
        }
        return contents;
    };

    const auto original_filelist = read_text(filelist);
    if (!original_filelist) return false;
    const auto prepared_filelist =
        PrepareHudEditorFilelist(*original_filelist);
    if (!prepared_filelist) {
        error = L"HUD Editor setup stopped because dx12filelist.txt has an "
            L"invalid UTF-16 byte layout:\n" + filelist.wstring();
        return false;
    }
    if (*prepared_filelist != *original_filelist &&
        !AtomicWriteWithBackup(filelist, *prepared_filelist, error)) {
        return false;
    }

    const auto original_input = read_text(input_settings);
    if (!original_input) return false;
    const std::string merged_input = MergeHudEditorBindings(*original_input);
    if (merged_input != *original_input &&
        !AtomicWriteWithBackup(input_settings, merged_input, error)) {
        return false;
    }

    // A successful ReplaceFile call is not enough evidence: antivirus,
    // synchronization software, or a running game can replace the file again.
    // Read both targets back and validate the exact state consumed by REDengine.
    const auto verified_filelist = read_text(filelist);
    if (!verified_filelist) return false;
    const auto verified_prepared_filelist =
        PrepareHudEditorFilelist(*verified_filelist);
    if (!verified_prepared_filelist ||
        *verified_prepared_filelist != *verified_filelist) {
        error = L"HUD Editor setup verification failed after writing:\n" +
            filelist.wstring() +
            L"\n\nThe required XML registration was not present in the file's "
            L"original encoding when it was read back.";
        return false;
    }

    const auto verified_input = read_text(input_settings);
    if (!verified_input) return false;
    if (MergeHudEditorBindings(*verified_input) != *verified_input) {
        error = L"HUD Editor setup verification failed after writing:\n" +
            input_settings.wstring() +
            L"\n\nOne or more required INS/F7 bindings were not present when "
            L"the file was read back. Close The Witcher 3 before retrying.";
        return false;
    }
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
    result.state.menu_scale = std::clamp(
        ReadFloat(*vr, "openxr", "menu_scale", 0.85f), 0.3f, 1.5f);
    result.state.cinema_scale = std::clamp(
        ReadFloat(*vr, "openxr", "cinema_scale", result.state.menu_scale),
        0.3f, 1.5f);
    result.state.cinema_hud_scale = std::clamp(
        ReadFloat(*vr, "openxr", "cinema_hud_scale", 1.30f), 0.5f, 1.5f);
    result.state.cinema_hud_convergence_offset = std::clamp(
        ReadInt(*vr, "openxr", "cinema_hud_stereo_shift_px", -72) -
            CinemaHudConvergenceShift(result.state.cinema_hud_scale, 0),
        -64, 64);
    result.state.full_vr_hud_scale = std::clamp(
        ReadFloat(*vr, "openxr", "full_vr_hud_scale", 1.00f), 0.5f, 1.5f);
    result.state.full_vr_hud_convergence_offset = std::clamp(
        ReadInt(*vr, "openxr", "full_vr_hud_stereo_shift_px", -36) -
            FullVrHudConvergenceShift(result.state.full_vr_hud_scale, 0),
        -64, 64);
    result.state.near_view = std::clamp(
        ReadFloat(*vr, "engine", "close_camera_offset", 0.75f), -2.0f, 3.0f);
    result.state.vertical_pitch_enabled = ReadBool(
        *vr, "openxr", "vertical_pitch_enabled", false);
    result.state.cinema_full_vr = ReadBool(
        *vr, "openxr", "cinema_full_vr", true);
    result.state.steady_icons = ReadBool(
        *vr, "openxr", "steady_icons", false);
    result.state.first_person_gamepad_head_follow =
        ReadBool(*vr, "engine", "first_person_snap_turn", false) &&
        ReadBool(*vr, "engine", "first_person_hmd_body_follow", false);
    const int snap_turn_degrees = ReadInt(
        *vr, "engine", "first_person_snap_turn_degrees", 45);
    result.state.first_person_snap_turn_degrees =
        snap_turn_degrees == 30 || snap_turn_degrees == 60
        ? snap_turn_degrees
        : 45;
    result.state.first_person_combat_exit = ReadBool(
        *vr, "engine", "first_person_combat_exit", true);
    result.state.first_person_stationary_turn = ReadBool(
        *vr, "engine", "first_person_stationary_turn", false);
    result.state.fast_movement_transitions = ReadBool(
        *game, "DLC", "DlcEnabled_movementinputfix", true);
    result.state.native_stereo = ReadBool(
        *vr, "openxr", "fullscreen_projection", false);
    result.state.diagnostic_logging =
        ReadBool(*vr, "debug", "logging_enabled", false) &&
        ReadBool(*vr, "debug", "runtime_diagnostics", false);
    return result;
}

CompatibilityWarnings InspectCompatibilitySettings(const ConfigPaths& paths) {
    CompatibilityWarnings warnings;
    std::wstring error;
    const auto game = IniDocument::Load(paths.game_settings, error);
    if (!game) return warnings;
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
    // Native asymmetric stereo is a launcher-owned experimental route. Keep
    // the underlying projection authority off for every other render mode.
    const bool native_stereo_active =
        state.native_stereo && state.mode == RenderMode::StereoNone;
    vr_ini.Set("openxr", "fullscreen_projection",
        native_stereo_active ? "1" : "0");
    vr_ini.Set("openxr", "hud_stereo_shift_px",
        std::to_string(std::clamp(state.hud_convergence_delta - 16, -256, 256)));
    vr_ini.Set("openxr", "presentation_scale", FloatString(
        native_stereo_active ? 1.0f : state.presentation_scale));
    vr_ini.Set("openxr", "menu_scale", FloatString(state.menu_scale));
    vr_ini.Set("openxr", "cinema_scale", FloatString(state.cinema_scale));
    vr_ini.Set("openxr", "cinema_hud_scale",
        FloatString(state.cinema_hud_scale));
    vr_ini.Set("openxr", "cinema_hud_stereo_shift_px",
        std::to_string(CinemaHudConvergenceShift(
            state.cinema_hud_scale,
            state.cinema_hud_convergence_offset)));
    vr_ini.Set("openxr", "full_vr_hud_scale",
        FloatString(state.full_vr_hud_scale));
    vr_ini.Set("openxr", "full_vr_hud_stereo_shift_px",
        std::to_string(FullVrHudConvergenceShift(
            state.full_vr_hud_scale,
            state.full_vr_hud_convergence_offset)));
    vr_ini.Set("openxr", "vertical_pitch_enabled",
        state.vertical_pitch_enabled ? "1" : "0");
    // Cinema framing is intentionally fixed in the launcher. Keep the INI key
    // because the renderer still consumes it and advanced users can inspect it.
    vr_ini.Set("openxr", "cinema_5x4", "1");
    vr_ini.Set("openxr", "cinema_full_vr",
        state.cinema_full_vr ? "1" : "0");
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
    const int snap_turn_degrees =
        state.first_person_snap_turn_degrees == 30 ||
        state.first_person_snap_turn_degrees == 60
        ? state.first_person_snap_turn_degrees
        : 45;
    vr_ini.Set("engine", "first_person_snap_turn_degrees",
        std::to_string(snap_turn_degrees));
    vr_ini.Set("engine", "first_person_combat_exit",
        state.first_person_combat_exit ? "1" : "0");
    vr_ini.Set("engine", "first_person_stationary_turn",
        state.first_person_stationary_turn ? "1" : "0");
    const bool dlss_dlaa = ModeUsesDlss(state.mode) && state.dlss_quality == 0;
    vr_ini.Set("engine", "dlss_dlaa", dlss_dlaa ? "1" : "0");
    // [DEBUG 1/2] One launcher switch owns the log writer and every bounded
    // runtime probe, so release and diagnostic runs use one DLL.
    vr_ini.Set("debug", "logging_enabled",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "runtime_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "taau_drop_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "cinema_camera_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "cinema_subtitle_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "first_person_state_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "first_person_aim_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    vr_ini.Set("debug", "world_marker_diagnostics",
        state.diagnostic_logging ? "1" : "0");
    // [FIX:FUNCTIONAL-REVERSE-ROUTE 2/2] Functional renderer hooks no longer
    // depend on the historical reverse master. Clear manual release-era
    // workarounds on every launcher save while preserving individual probes.
    vr_ini.Set("reverse", "enabled", "0");
    // Remove the two pre-release compositor snap-turn keys. V831/V838 use the
    // first-person native-camera controls in [engine]; retaining these ignored
    // aliases makes migrated user INIs misleading.
    // Remove distributed trial keys that no longer have a runtime consumer.
    // Keep all other unmanaged renderer values intact.
    RemoveObsoleteSettings(vr_ini);
    vr_ini.Set("meta", "config_version", std::to_string(kCurrentConfigVersion));

    game_settings.Set("Viewport", "Resolution", "\"" +
        std::to_string(state.width) + "x" + std::to_string(state.height) + "\"");
    game_settings.Set("PostProcess", "AAMode", std::to_string(mode.aa_mode));
    game_settings.Set("PostProcess", "DLSSQuality",
        std::to_string(state.dlss_quality == 0
            ? 1
            : std::clamp(state.dlss_quality, 1, 4)));
    game_settings.Set("Rendering", "AllowDLSS",
        mode.allow_dlss ? "true" : "false");
    // The DLC remains installed; REDengine's native DLC switch keeps its
    // animation-behavior mounter dormant when the launcher option is disabled.
    game_settings.Set("DLC", "DlcEnabled_movementinputfix",
        state.fast_movement_transitions ? "1" : "0");
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
    // Graphics configuration must never select the user's spoken or text
    // language. Preserve the complete existing localization section instead.
    for (const auto& [key, value] : current->Entries("Localization")) {
        configured.Set("Localization", key, value);
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
