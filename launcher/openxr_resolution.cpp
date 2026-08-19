#include "openxr_resolution.h"

#include <Windows.h>

#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>

#include <cstring>
#include <vector>

namespace w3vr {
namespace {

template <typename Function>
bool LoadOpenXrFunction(
    PFN_xrGetInstanceProcAddr get_instance_proc_addr,
    XrInstance instance,
    const char* name,
    Function& function) {
    PFN_xrVoidFunction raw{};
    if (get_instance_proc_addr == nullptr ||
        XR_FAILED(get_instance_proc_addr(instance, name, &raw)) ||
        raw == nullptr) {
        return false;
    }
    function = reinterpret_cast<Function>(raw);
    return true;
}

std::wstring Utf8ToWide(const char* text) {
    if (text == nullptr || *text == '\0') {
        return {};
    }
    const int characters = MultiByteToWideChar(
        CP_UTF8, 0, text, -1, nullptr, 0);
    if (characters <= 1) {
        return {};
    }
    std::wstring result(static_cast<size_t>(characters), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text, -1, result.data(), characters);
    result.pop_back();
    return result;
}

std::wstring OpenXrFailure(const wchar_t* operation, XrResult result) {
    return std::wstring(operation) + L" failed (OpenXR result " +
        std::to_wstring(static_cast<int64_t>(result)) + L").";
}

} // namespace

bool QueryOpenXrRecommendedResolution(
    const std::filesystem::path& launcher_directory,
    OpenXrRecommendedResolution& resolution,
    std::wstring& error) {
    resolution = {};
    error.clear();

    const auto adjacent_loader = launcher_directory / L"openxr_loader.dll";
    HMODULE loader = LoadLibraryW(adjacent_loader.c_str());
    if (loader == nullptr) {
        loader = LoadLibraryW(L"openxr_loader.dll");
    }
    if (loader == nullptr) {
        error = L"OpenXR AUTO could not load openxr_loader.dll. Keep the "
            L"project loader beside Witcher3VRLauncher.exe.";
        return false;
    }

    const auto get_instance_proc_addr =
        reinterpret_cast<PFN_xrGetInstanceProcAddr>(
            GetProcAddress(loader, "xrGetInstanceProcAddr"));
    PFN_xrCreateInstance create_instance{};
    if (!LoadOpenXrFunction(
            get_instance_proc_addr, XR_NULL_HANDLE,
            "xrCreateInstance", create_instance)) {
        error = L"OpenXR AUTO could not resolve xrCreateInstance.";
        FreeLibrary(loader);
        return false;
    }

    XrInstanceCreateInfo create_info{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(
        create_info.applicationInfo.applicationName,
        "Witcher 3 VR Launcher");
    create_info.applicationInfo.applicationVersion = 1242;
    strcpy_s(create_info.applicationInfo.engineName, "Witcher3VR");
    create_info.applicationInfo.engineVersion = 1242;
    create_info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);

    XrInstance instance{XR_NULL_HANDLE};
    XrResult xr_result = create_instance(&create_info, &instance);
    if (XR_FAILED(xr_result) || instance == XR_NULL_HANDLE) {
        error = OpenXrFailure(L"Creating the OpenXR query instance", xr_result);
        FreeLibrary(loader);
        return false;
    }

    PFN_xrDestroyInstance destroy_instance{};
    PFN_xrGetSystem get_system{};
    PFN_xrEnumerateViewConfigurationViews enumerate_views{};
    PFN_xrGetInstanceProperties get_instance_properties{};
    const bool functions_ready = LoadOpenXrFunction(
            get_instance_proc_addr, instance,
            "xrDestroyInstance", destroy_instance) &&
        LoadOpenXrFunction(
            get_instance_proc_addr, instance, "xrGetSystem", get_system) &&
        LoadOpenXrFunction(
            get_instance_proc_addr, instance,
            "xrEnumerateViewConfigurationViews", enumerate_views);
    LoadOpenXrFunction(
        get_instance_proc_addr, instance,
        "xrGetInstanceProperties", get_instance_properties);
    if (!functions_ready) {
        error = L"OpenXR AUTO could not resolve the view-configuration APIs.";
        if (destroy_instance != nullptr) {
            destroy_instance(instance);
        }
        FreeLibrary(loader);
        return false;
    }

    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system_id{XR_NULL_SYSTEM_ID};
    xr_result = get_system(instance, &system_info, &system_id);
    if (XR_FAILED(xr_result) || system_id == XR_NULL_SYSTEM_ID) {
        error = OpenXrFailure(
            L"Finding the active OpenXR headset", xr_result);
        destroy_instance(instance);
        FreeLibrary(loader);
        return false;
    }

    uint32_t view_count{};
    xr_result = enumerate_views(
        instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        0, &view_count, nullptr);
    if (XR_FAILED(xr_result) || view_count == 0) {
        error = OpenXrFailure(
            L"Reading the OpenXR stereo view count", xr_result);
        destroy_instance(instance);
        FreeLibrary(loader);
        return false;
    }

    std::vector<XrViewConfigurationView> views(
        view_count, XrViewConfigurationView{
            XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xr_result = enumerate_views(
        instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        view_count, &view_count, views.data());
    if (XR_FAILED(xr_result) || view_count == 0 ||
        views[0].recommendedImageRectWidth == 0 ||
        views[0].recommendedImageRectHeight == 0) {
        error = OpenXrFailure(
            L"Reading the OpenXR recommended resolution", xr_result);
        destroy_instance(instance);
        FreeLibrary(loader);
        return false;
    }

    resolution.width = static_cast<int>(
        views[0].recommendedImageRectWidth);
    resolution.height = static_cast<int>(
        views[0].recommendedImageRectHeight);
    if (get_instance_properties != nullptr) {
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        if (XR_SUCCEEDED(get_instance_properties(instance, &properties))) {
            resolution.runtime_name = Utf8ToWide(properties.runtimeName);
        }
    }

    destroy_instance(instance);
    FreeLibrary(loader);
    return true;
}

} // namespace w3vr
