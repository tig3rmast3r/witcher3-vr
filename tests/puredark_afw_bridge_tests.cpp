#include "puredark_afw_bridge.h"

#include <cstdio>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

bool make_d3d12_context(
    ComPtr<ID3D12Device>& device,
    ComPtr<ID3D12CommandQueue>& queue) {
    HRESULT result = D3D12CreateDevice(
        nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(result)) {
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> warp_adapter;
        result = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(result) ||
            FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter))) ||
            FAILED(D3D12CreateDevice(
                warp_adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)))) {
            return false;
        }
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    return SUCCEEDED(device->CreateCommandQueue(
        &queue_desc, IID_PPV_ARGS(&queue)));
}

bool make_probe_texture(
    ID3D12Device* device, ComPtr<ID3D12Resource>& texture) {
    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CreationNodeMask = 1;
    heap_properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Width = 64;
    resource_desc.Height = 64;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    return SUCCEEDED(device->CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture)));
}

bool make_command_context(
    ID3D12Device* device,
    ComPtr<ID3D12CommandAllocator>& allocator,
    ComPtr<ID3D12GraphicsCommandList>& command_list) {
    return SUCCEEDED(device->CreateCommandAllocator(
               D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
        SUCCEEDED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&command_list)));
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    using namespace w3vr::puredark_afw;

    Bridge bridge;
    std::wstring error;
    if (bridge.load(L"Z:\\w3vr-missing\\PDAFWPlugin.dll", error) ||
        error.empty() || bridge.loaded()) {
        std::fputs("missing DLL failure contract is broken\n", stderr);
        return 1;
    }

    if (argc >= 2) {
        error.clear();
        if (!bridge.load(argv[1], error) || !bridge.loaded()) {
            std::fwprintf(stderr, L"AFW export probe failed: %ls\n", error.c_str());
            return 1;
        }

        TextureDesc invalid_texture{};
        if (bridge.setup_texture_desc(invalid_texture, error) || error.empty()) {
            std::fputs("uninitialized descriptor contract is broken\n", stderr);
            return 1;
        }

        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;
        if (!make_d3d12_context(device, queue)) {
            std::fputs("could not create a D3D12 probe context\n", stderr);
            return 1;
        }

        DeviceParams device_params{};
        device_params.d3d12_device = device.Get();
        device_params.d3d12_queue = queue.Get();
        if (!bridge.initialize_device(device_params, error)) {
            std::fwprintf(stderr, L"AFW InitDevice probe failed: %ls\n", error.c_str());
            return 1;
        }
        if (bridge.begin_command_list(0, error) == nullptr ||
            !bridge.end_command_list(0, error)) {
            std::fwprintf(
                stderr, L"AFW owned command-list probe failed: %ls\n",
                error.c_str());
            return 1;
        }

        ComPtr<ID3D12Resource> texture;
        if (!make_probe_texture(device.Get(), texture)) {
            std::fputs("could not create the D3D12 probe texture\n", stderr);
            return 1;
        }
        TextureDesc texture_desc{};
        texture_desc.texture = texture.Get();
        texture_desc.initial_state = D3D12_RESOURCE_STATE_COMMON;
        if (!bridge.setup_texture_desc(texture_desc, error)) {
            std::fwprintf(
                stderr, L"AFW SetupTextureDesc probe failed: %ls\n",
                error.c_str());
            return 1;
        }

        TextureDesc copied_texture{};
        if (!bridge.create_texture(
                64, 64, DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                copied_texture, true, error)) {
            std::fwprintf(
                stderr, L"AFW CreateTexture probe failed: %ls\n",
                error.c_str());
            return 1;
        }
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> command_list;
        if (!make_command_context(
                device.Get(), allocator, command_list)) {
            std::fputs("could not create the D3D12 probe command list\n", stderr);
            return 1;
        }
        const D3D12_BOX full_box{0, 0, 0, 64, 64, 1};
        if (!bridge.copy_texture(
                command_list.Get(), copied_texture, texture_desc,
                full_box, 0, 0, error) ||
            FAILED(command_list->Close())) {
            std::fwprintf(
                stderr, L"AFW Copy probe failed: %ls\n", error.c_str());
            return 1;
        }
        ID3D12CommandList* command_lists[]{command_list.Get()};
        queue->ExecuteCommandLists(1, command_lists);

        FrameWarpInitParams frame_warp_params{};
        frame_warp_params.hmd_width = 64;
        frame_warp_params.hmd_height = 64;
        EyeFrameBuffers frame_buffers{};
        if (!bridge.initialize_frame_warp(
                frame_warp_params, frame_buffers, error)) {
            std::fwprintf(
                stderr, L"AFW InitFrameWarp probe failed: %ls\n",
                error.c_str());
            return 1;
        }

        std::printf(
            "PureDark D3D12 ABI probe passed: srv=%d uav=%d created=%p\n",
            texture_desc.srv_position, texture_desc.uav_position,
            copied_texture.texture);
        bridge.unload();
        if (bridge.loaded()) {
            std::fputs("AFW unload contract is broken\n", stderr);
            return 1;
        }
    }

    std::puts("PureDark AFW bridge tests passed");
    return 0;
}
