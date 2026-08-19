#pragma once

#include <filesystem>
#include <string>

namespace w3vr {

struct OpenXrRecommendedResolution {
    int width{};
    int height{};
    std::wstring runtime_name{};
};

// Opens the configured OpenXR runtime without creating a graphics session and
// reads the first PRIMARY_STEREO view's current recommended image rectangle.
// The launcher calls this only immediately before saving/launching.
bool QueryOpenXrRecommendedResolution(
    const std::filesystem::path& launcher_directory,
    OpenXrRecommendedResolution& resolution,
    std::wstring& error);

} // namespace w3vr
