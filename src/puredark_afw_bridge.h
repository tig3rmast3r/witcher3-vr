#pragma once

#include <Windows.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_2.h>

#include <cstddef>
#include <string>
#include <type_traits>

namespace w3vr::puredark_afw {

// Local binary-interface declarations for PDAFWPlugin.dll v1.0-beta.5. The
// PureDark header is deliberately not copied or included by the renderer.
struct Matrix4x4 {
    float values[16]{};
};

struct DeviceParams {
    ID3D11Device* d3d11_device{};
    ID3D11DeviceContext* d3d11_context{};
    ID3D12Device* d3d12_device{};
    ID3D12CommandQueue* d3d12_queue{};
    IDXGIAdapter* dxgi_adapter{};
};

struct FrameWarpInitParams {
    int hmd_width{};
    int hmd_height{};
    DXGI_FORMAT eye_format{DXGI_FORMAT_R8G8B8A8_UNORM};
    DXGI_FORMAT backbuffer_format{DXGI_FORMAT_R10G10B10A2_UNORM};
};

enum EyeIndex {
    EyeLeft = 0,
    EyeRight = 1,
};

enum FrameWarpMode {
    None = 0,
    AlternateEyeWarping = 1,
    PreviousFrameWarping = 2,
    CombinedWarping = 3,
};

enum ImageType {
    Image = 0,
    Depth = 1,
};

struct TextureDesc {
    TextureDesc() = default;
    ImageType type{Image};
    ID3D12Resource* texture{};
    int srv_position{-1};
    int uav_position{-1};
    D3D12_GPU_DESCRIPTOR_HANDLE shader_resource_view{};
    D3D12_GPU_DESCRIPTOR_HANDLE unordered_access_view{};
    union {
        D3D12_CPU_DESCRIPTOR_HANDLE render_target_view{};
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view;
    };
    D3D12_RESOURCE_STATES initial_state{D3D12_RESOURCE_STATE_COMMON};
};

struct FrameBufferDesc {
    FrameBufferDesc() = default;
    TextureDesc color;
    TextureDesc depth;
    TextureDesc motion_vectors;
};

struct EyeFrameBuffers {
    FrameBufferDesc eye_frame_buffers[2];
};

struct CameraData {
    Matrix4x4 destination_world_to_view;
    Matrix4x4 destination_view_to_world;
    Matrix4x4 destination_view_to_clip;
    Matrix4x4 destination_clip_to_view;
    Matrix4x4 source_world_to_view;
    Matrix4x4 source_view_to_world;
    Matrix4x4 source_view_to_clip;
    Matrix4x4 source_clip_to_view;
    Matrix4x4 camera_world_to_view;
    Matrix4x4 camera_view_to_world;
    Matrix4x4 camera_view_to_clip;
    Matrix4x4 camera_clip_to_view;
};

enum MotionVectorType {
    Normal = 0,
    FromOtherEye = 1,
    ObjectOnly = 2,
};

struct FrameWarpEvaluateParams {
    void* command_list{};
    FrameBufferDesc* input_eye_frame_buffer{};
    FrameBufferDesc* output_eye_frame_buffer{};
    TextureDesc* ui_color_alpha{};
    float ui_scale[2]{1.0f, 1.0f};
    float ui_position[3]{0.0f, 0.0f, -1.0f};
    float motion_scale[2]{};
    FrameWarpMode mode{};
    EyeIndex eye_index{};
    CameraData* camera_data{};
    bool clear_before_warping{};
    float ignore_motion_threshold{2.5f};
    bool is_hudless_color{true};
    MotionVectorType motion_vectors_type{Normal};
    bool debug{};
    bool is_foveated{};
    RECT foveated_area{};
    TextureDesc* ue_velocity_buffer{};
    bool use_uint64{};
    float reserved[12]{};
};

static_assert(sizeof(Matrix4x4) == 64);
static_assert(sizeof(DeviceParams) == 40);
static_assert(sizeof(FrameWarpInitParams) == 16);
static_assert(sizeof(TextureDesc) == 56);
static_assert(sizeof(FrameBufferDesc) == 168);
static_assert(sizeof(EyeFrameBuffers) == 336);
static_assert(sizeof(CameraData) == 768);
static_assert(sizeof(FrameWarpEvaluateParams) == 184);
static_assert(offsetof(FrameWarpEvaluateParams, camera_data) == 72);
static_assert(offsetof(FrameWarpEvaluateParams, foveated_area) == 100);
static_assert(offsetof(FrameWarpEvaluateParams, ue_velocity_buffer) == 120);
static_assert(offsetof(FrameWarpEvaluateParams, reserved) == 132);
static_assert(std::is_trivially_copyable_v<EyeFrameBuffers>);

struct D3D12RendererApi;

class Bridge {
public:
    Bridge() = default;
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;
    ~Bridge();

    bool load(const std::wstring& dll_path, std::wstring& error);
    void unload();
    bool loaded() const;

    bool initialize_device(
        const DeviceParams& params, std::wstring& error);
    bool initialize_frame_warp(
        const FrameWarpInitParams& params,
        EyeFrameBuffers& frame_buffers,
        std::wstring& error) const;
    ID3D12GraphicsCommandList* begin_command_list(
        int index, std::wstring& error) const;
    bool end_command_list(int index, std::wstring& error) const;
    bool setup_texture_desc(
        TextureDesc& texture_desc, std::wstring& error) const;
    bool create_texture(
        int width,
        int height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_STATES initial_state,
        TextureDesc& texture_desc,
        bool create_uav,
        std::wstring& error) const;
    bool copy_texture(
        ID3D12GraphicsCommandList* command_list,
        TextureDesc& destination,
        TextureDesc& source,
        const D3D12_BOX& source_box,
        UINT destination_x,
        UINT destination_y,
        std::wstring& error) const;
    bool evaluate(
        FrameWarpEvaluateParams& params, std::wstring& error) const;

private:
    using InitDeviceFn = D3D12RendererApi* (__stdcall*)(DeviceParams);
    using InitFrameWarpFn = EyeFrameBuffers (__stdcall*)(FrameWarpInitParams);
    using EvaluateFrameWarpFn = void (__stdcall*)(FrameWarpEvaluateParams&);

    HMODULE module_{};
    InitDeviceFn init_device_{};
    InitFrameWarpFn init_frame_warp_{};
    EvaluateFrameWarpFn evaluate_frame_warp_{};
    D3D12RendererApi* renderer_api_{};
};

}  // namespace w3vr::puredark_afw
