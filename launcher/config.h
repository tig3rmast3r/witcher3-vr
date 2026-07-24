#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace w3vr {

enum class RenderMode {
    MonoNone,
    MonoTaau,
    MonoDlss,
    StereoNone,
    StereoTaau,
    StereoDlssPacked,
};

struct ModeSettings {
    int openxr_mode{};
    bool dual_render{};
    const char* temporal_backend{};
    int aa_mode{};
    bool allow_dlss{};
};

struct LauncherState {
    RenderMode mode{RenderMode::StereoNone};
    int width{2688};
    int height{2784};
    int dlss_quality{3};
    int hud_convergence_delta{};
    float presentation_scale{1.0f};
    float hud_horizontal_scale{0.5f};
    float hud_vertical_scale{0.5f};
    float menu_scale{0.85f};
    float cinema_scale{0.9f};
    float near_view{0.75f};
    bool vertical_pitch_enabled{};
    bool cinema_5x4{};
    bool diagnostic_logging{};
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

ConfigPaths DiscoverPaths();
LoadResult LoadConfiguration(const ConfigPaths& paths);
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
