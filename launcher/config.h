#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace w3vr {

enum class RenderMode {
    AerAfwTaau,
    AerAfwDlss,
    StereoNone,
    StereoTaau,
    StereoDlssSequential,
    Count,
};

enum class CinemaAspect {
    FiveFour,
    FourThree,
};

enum class CameraFollowPolicy {
    AlwaysOn,
    HorseBoatOnly,
    AlwaysOff,
};

constexpr bool CameraFollowEnabled(
    CameraFollowPolicy policy, bool horse_or_boat, bool first_person) {
    if (policy == CameraFollowPolicy::AlwaysOff) return false;
    if (horse_or_boat) return true;
    return policy == CameraFollowPolicy::AlwaysOn && !first_person;
}

constexpr bool StaticHudModuleVisible(
    bool policy_enabled, bool in_combat, bool witcher_sense,
    bool horse_racing, bool navigation_module) {
    return !policy_enabled || in_combat || witcher_sense ||
        (navigation_module && horse_racing);
}

struct ModeSettings {
    int openxr_mode{};
    bool dual_render{};
    bool mode3_aer_presentation{};
    const char* temporal_backend{};
    int aa_mode{};
    bool allow_dlss{};
};

struct LauncherState {
    RenderMode mode{RenderMode::StereoNone};
    bool resolution_auto{true};
    int width{2688};
    int height{2784};
    int dlss_quality{1};
    bool ray_tracing{};
    int hud_convergence_delta{-20};
    float presentation_scale{1.0f};
    float menu_scale{0.85f};
    float cinema_scale{0.9f};
    CinemaAspect cinema_aspect{CinemaAspect::FiveFour};
    float cinema_hud_scale{1.30f};
    int cinema_hud_convergence_offset{};
    float full_vr_hud_scale{1.00f};
    int full_vr_hud_convergence_offset{};
    float near_view{0.75f};
    bool vertical_pitch_enabled{};
    bool cinema_full_vr{true};
    bool steady_icons{};
    bool first_person_gamepad_head_follow{};
    int first_person_snap_turn_degrees{45};
    bool first_person_combat_exit{false};
    bool first_person_strafe{true};
    bool first_person_anchor_smoothing{true};
    CameraFollowPolicy camera_follow_policy{CameraFollowPolicy::HorseBoatOnly};
    bool hide_static_hud_outside_combat{};
    bool fast_movement_transitions{true};
    bool native_stereo{};
    bool fullscreen_projection{};
    bool alternate_presentation_resize{};
    bool diagnostic_logging{};
};

struct CompatibilityWarnings {
    bool ray_tracing_enabled{};
    bool ssr_high{};
};

struct ConfigPaths {
    std::filesystem::path launcher_directory;
    std::filesystem::path vr_ini;
    std::filesystem::path game_settings;
    std::filesystem::path game_executable;
};

struct LoadResult {
    LauncherState state;
    std::wstring warning;
};

class IniDocument {
public:
    static std::optional<IniDocument> Load(
        const std::filesystem::path& path, std::wstring& error);
    static IniDocument FromText(std::string text);

    std::optional<std::string> Get(
        const std::string& section, const std::string& key) const;
    std::vector<std::pair<std::string, std::string>> Entries(
        const std::string& section) const;
    void Set(const std::string& section, const std::string& key,
        const std::string& value);
    void Remove(const std::string& section, const std::string& key);
    void FillMissingFrom(const IniDocument& defaults);
    std::string Serialize() const;

private:
    struct Line {
        std::string text;
        std::string newline;
    };
    std::vector<Line> lines_;
    std::string default_newline_{"\r\n"};
};

const ModeSettings& SettingsForMode(RenderMode mode);
const wchar_t* ModeDisplayName(RenderMode mode);
bool ModeUsesDlss(RenderMode mode);
bool ModeUsesStereo(RenderMode mode);
bool ModeSupportsRayTracing(RenderMode mode);
bool AlternatePresentationResizeAvailable(
    RenderMode mode, bool native_stereo, float presentation_scale);
std::optional<int> DlssNearSquareCompatibleWidth(const LauncherState& state);
int CinemaHudConvergenceShift(float hud_scale, int offset);
int FullVrHudConvergenceShift(float hud_scale, int offset);

ConfigPaths DiscoverPaths();
bool EnsureVrConfiguration(const ConfigPaths& paths,
    const std::string& template_contents, bool& created, std::wstring& error);
bool EnsureHudEditorSetup(const ConfigPaths& paths, std::wstring& error);
std::wstring HudEditorManualSetupInstructions(const ConfigPaths& paths);
LoadResult LoadConfiguration(const ConfigPaths& paths);
CompatibilityWarnings InspectCompatibilitySettings(const ConfigPaths& paths);
bool BuildUpdatedDocuments(const ConfigPaths& paths, const LauncherState& state,
    IniDocument& vr_ini, IniDocument& game_settings, std::wstring& error);
bool SaveConfiguration(const ConfigPaths& paths, const LauncherState& state,
    std::wstring& error);
bool AtomicWriteWithBackup(const std::filesystem::path& path,
    const std::string& contents, std::wstring& error);
std::filesystem::path OriginalSettingsBackupPath(const ConfigPaths& paths);
bool HasOriginalSettingsBackup(const ConfigPaths& paths);
bool ConfigureGameSettingsForVr(const ConfigPaths& paths,
    const std::string& template_contents, std::wstring& error);
bool RestoreOriginalGameSettings(const ConfigPaths& paths, std::wstring& error);

} // namespace w3vr
