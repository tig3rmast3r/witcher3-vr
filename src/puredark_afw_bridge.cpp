#include "puredark_afw_bridge.h"

#include <sstream>

namespace w3vr::puredark_afw {
namespace {

// PDAFWPlugin v1.0-beta.5 exposes D3D12RendererAPI as a C++ interface returned
// by InitDevice. SetupTextureDesc is vtable slot 14: the preceding slots are
// Begin/End/Wait, device/queue/heap, RTV/DSV, SRV/UAV/CBV, and the three
// descriptor-handle accessors. Keep this isolated behind the bridge instead of
// publishing the upstream all-rights-reserved interface in renderer sources.
constexpr size_t kSetupTextureDescVtableSlot = 14;
constexpr size_t kCreateTextureVtableSlot = 16;
constexpr size_t kCopyTextureVtableSlot = 20;
using SetupTextureDescFn = void(__fastcall*)(
    D3D12RendererApi*, TextureDesc&);
using CreateTextureFn = bool(__fastcall*)(
    D3D12RendererApi*, int, int, DXGI_FORMAT,
    D3D12_RESOURCE_STATES, TextureDesc&, bool);
using CopyTextureFn = void(__fastcall*)(
    D3D12RendererApi*, ID3D12GraphicsCommandList*, TextureDesc&,
    TextureDesc&, D3D12_BOX, UINT, UINT);

std::wstring windows_error(const wchar_t* operation, DWORD code) {
    std::wostringstream stream;
    stream << operation << L" failed with Windows error " << code;
    return stream.str();
}

}  // namespace

Bridge::~Bridge() {
    unload();
}

bool Bridge::load(const std::wstring& dll_path, std::wstring& error) {
    unload();
    if (dll_path.empty()) {
        error = L"PDAFWPlugin.dll path is empty";
        return false;
    }

    module_ = LoadLibraryExW(
        dll_path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (module_ == nullptr) {
        error = windows_error(L"Loading PDAFWPlugin.dll", GetLastError());
        return false;
    }

    init_device_ = reinterpret_cast<InitDeviceFn>(
        GetProcAddress(module_, "InitDevice"));
    init_frame_warp_ = reinterpret_cast<InitFrameWarpFn>(
        GetProcAddress(module_, "InitFrameWarp"));
    evaluate_frame_warp_ = reinterpret_cast<EvaluateFrameWarpFn>(
        GetProcAddress(module_, "EvaluateFrameWarp"));
    if (init_device_ == nullptr || init_frame_warp_ == nullptr ||
        evaluate_frame_warp_ == nullptr) {
        error = L"PDAFWPlugin.dll is missing one or more required exports: "
            L"InitDevice, InitFrameWarp, EvaluateFrameWarp";
        unload();
        return false;
    }
    error.clear();
    return true;
}

void Bridge::unload() {
    renderer_api_ = nullptr;
    init_device_ = nullptr;
    init_frame_warp_ = nullptr;
    evaluate_frame_warp_ = nullptr;
    if (module_ != nullptr) {
        FreeLibrary(module_);
        module_ = nullptr;
    }
}

bool Bridge::loaded() const {
    return module_ != nullptr && init_device_ != nullptr &&
        init_frame_warp_ != nullptr && evaluate_frame_warp_ != nullptr;
}

bool Bridge::initialize_device(
    const DeviceParams& params, std::wstring& error) {
    if (!loaded()) {
        error = L"PDAFWPlugin.dll is not loaded";
        return false;
    }
    renderer_api_ = init_device_(params);
    if (renderer_api_ == nullptr) {
        error = L"PureDark InitDevice returned null";
        return false;
    }
    error.clear();
    return true;
}

bool Bridge::initialize_frame_warp(
    const FrameWarpInitParams& params,
    EyeFrameBuffers& frame_buffers,
    std::wstring& error) const {
    if (!loaded() || renderer_api_ == nullptr) {
        error = L"PureDark device is not initialized";
        return false;
    }
    frame_buffers = init_frame_warp_(params);
    if (frame_buffers.eye_frame_buffers[0].color.texture == nullptr ||
        frame_buffers.eye_frame_buffers[1].color.texture == nullptr) {
        error = L"PureDark InitFrameWarp returned incomplete eye buffers";
        return false;
    }
    error.clear();
    return true;
}

bool Bridge::setup_texture_desc(
    TextureDesc& texture_desc, std::wstring& error) const {
    if (!loaded() || renderer_api_ == nullptr) {
        error = L"PureDark device is not initialized";
        return false;
    }
    if (texture_desc.texture == nullptr) {
        error = L"PureDark texture descriptor requires a D3D12 resource";
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(renderer_api_);
    if (vtable == nullptr || vtable[kSetupTextureDescVtableSlot] == nullptr) {
        error = L"PureDark D3D12RendererAPI has no SetupTextureDesc entry";
        return false;
    }
    const auto setup_texture_desc = reinterpret_cast<SetupTextureDescFn>(
        vtable[kSetupTextureDescVtableSlot]);
    setup_texture_desc(renderer_api_, texture_desc);
    if (texture_desc.srv_position < 0 ||
        texture_desc.shader_resource_view.ptr == 0) {
        error = L"PureDark SetupTextureDesc produced no shader resource view";
        return false;
    }
    error.clear();
    return true;
}

bool Bridge::create_texture(
    int width,
    int height,
    DXGI_FORMAT format,
    D3D12_RESOURCE_STATES initial_state,
    TextureDesc& texture_desc,
    bool create_uav,
    std::wstring& error) const {
    if (!loaded() || renderer_api_ == nullptr) {
        error = L"PureDark device is not initialized";
        return false;
    }
    if (width <= 0 || height <= 0 || format == DXGI_FORMAT_UNKNOWN) {
        error = L"PureDark CreateTexture requires valid dimensions and format";
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(renderer_api_);
    if (vtable == nullptr || vtable[kCreateTextureVtableSlot] == nullptr) {
        error = L"PureDark D3D12RendererAPI has no CreateTexture entry";
        return false;
    }
    const auto create_texture = reinterpret_cast<CreateTextureFn>(
        vtable[kCreateTextureVtableSlot]);
    TextureDesc created{};
    if (!create_texture(
            renderer_api_, width, height, format, initial_state,
            created, create_uav) || created.texture == nullptr) {
        error = L"PureDark CreateTexture failed";
        return false;
    }
    texture_desc = created;
    if (texture_desc.initial_state != initial_state) {
        texture_desc.initial_state = initial_state;
    }
    error.clear();
    return true;
}

bool Bridge::copy_texture(
    ID3D12GraphicsCommandList* command_list,
    TextureDesc& destination,
    TextureDesc& source,
    const D3D12_BOX& source_box,
    UINT destination_x,
    UINT destination_y,
    std::wstring& error) const {
    if (!loaded() || renderer_api_ == nullptr) {
        error = L"PureDark device is not initialized";
        return false;
    }
    if (command_list == nullptr || destination.texture == nullptr ||
        source.texture == nullptr) {
        error = L"PureDark Copy requires a command list and two textures";
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(renderer_api_);
    if (vtable == nullptr || vtable[kCopyTextureVtableSlot] == nullptr) {
        error = L"PureDark D3D12RendererAPI has no Copy entry";
        return false;
    }
    const auto copy_texture = reinterpret_cast<CopyTextureFn>(
        vtable[kCopyTextureVtableSlot]);
    copy_texture(
        renderer_api_, command_list, destination, source, source_box,
        destination_x, destination_y);
    error.clear();
    return true;
}

bool Bridge::evaluate(
    FrameWarpEvaluateParams& params, std::wstring& error) const {
    if (!loaded() || renderer_api_ == nullptr) {
        error = L"PureDark device is not initialized";
        return false;
    }
    if (params.input_eye_frame_buffer == nullptr ||
        params.camera_data == nullptr) {
        error = L"PureDark evaluation requires input buffers and camera data";
        return false;
    }
    evaluate_frame_warp_(params);
    if (params.output_eye_frame_buffer == nullptr ||
        params.output_eye_frame_buffer->color.texture == nullptr) {
        error = L"PureDark EvaluateFrameWarp returned no output buffer";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace w3vr::puredark_afw
