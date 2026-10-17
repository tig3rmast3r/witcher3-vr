#pragma once

// Included after the retained-HUD compositor so this small diagnostic panel
// can reuse its established projection-slice shader and alpha-blend pipeline.
namespace cutscene_controls {
constexpr int kWidth = 640;
constexpr int kHeight = 460;
constexpr int kTrackLeft = 200;
constexpr int kTrackRight = 590;
constexpr int kFirstRow = 105;
constexpr int kRowStep = 65;
struct Slider {
    const wchar_t* label;
    const char* key;
    std::atomic<float>* value;
    float minimum;
    float maximum;
    bool degrees{};
};
inline Slider sliders[] = {
    {L"VR zoom", "cinema_fullscreen_zoom", &g_cinema_fullscreen_zoom, 0.5f, 4.0f},
    {L"World scale", "cinema_zoom_stereo_adjustment", &g_cutscene_zoom_stereo_adjustment, 0.0f, 1.0f},
    {L"Pitch height", "cinema_pitch_lowering", &g_cutscene_pitch_lowering, -0.3f, 0.4f},
    {L"HUD text size", "cinema_dialog_size", &g_cutscene_hud_size, 0.15f, 1.2f},
    {L"HUD height", "cinema_dialog_lowering", &g_cutscene_hud_lowering, -0.3f, 0.3f},
};
inline POINT cursor{-100, -100};
// Updated from the actual stereo-projected panel each rendered frame.
inline float mouse_left = 0.35f, mouse_top = 0.35f;
inline float mouse_width = 0.30f, mouse_height = 0.30f;
inline HWND game_window{};
inline WNDPROC previous_window_proc{};
inline int dragging = -1;
inline bool mouse_latched{};

inline LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (g_cutscene_controls_visible.load(std::memory_order_relaxed)) {
        // Keep slider clicks and raw mouse movement out of dialogue choices
        // and game-camera input. Keyboard/controller handling is unchanged.
        if (message == WM_INPUT) return DefWindowProcW(window, message, w, l);
        if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) return 0;
    }
    return CallWindowProcW(previous_window_proc, window, message, w, l);
}

inline void save() {
    for (const auto& slider : sliders) {
        write_ini_float("openxr", slider.key,
            slider.value->load(std::memory_order_relaxed));
    }
    log_line("Fullscreen controls saved zoom=%.3f pitch=%.3f projection=%.3f hud_size=%.3f hud_height=%.3f yaw_degrees=%.3f world_scale_adjustment=%.3f",
        g_cinema_fullscreen_zoom.load(), g_cutscene_pitch_lowering.load(),
        g_cutscene_projection_lowering.load(), g_cutscene_hud_size.load(),
        g_cutscene_hud_lowering.load(), g_cutscene_yaw_adjustment.load(),
        g_cutscene_zoom_stereo_adjustment.load());
}

inline void input(IDXGISwapChain* swapchain) {
    static bool toggle_latched{};
    const bool down = (GetAsyncKeyState(VK_MULTIPLY) & 0x8000) != 0;
    const bool pressed = down && !toggle_latched;
    toggle_latched = down;
    const bool eligible = g_config.cinema_fullscreen &&
        g_automatic_full_vr_camera_active.load(std::memory_order_acquire) &&
        !g_force_mono_cinema.load(std::memory_order_relaxed) &&
        g_engine_menu_state.load(std::memory_order_relaxed) == 0;
    if (!eligible) {
        if (g_cutscene_controls_visible.exchange(false)) save();
        dragging = -1;
        return;
    }
    DXGI_SWAP_CHAIN_DESC desc{};
    if (swapchain == nullptr || FAILED(swapchain->GetDesc(&desc))) return;
    const bool foreground = GetForegroundWindow() == desc.OutputWindow;
    if (pressed && foreground) {
        if (game_window == nullptr) {
            game_window = desc.OutputWindow;
            previous_window_proc = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(game_window, GWLP_WNDPROC));
            if (!previous_window_proc) { game_window = nullptr; return; }
            SetLastError(0);
            const LONG_PTR result = SetWindowLongPtrW(game_window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(window_proc));
            if (result == 0 && GetLastError() != 0) {
                game_window = nullptr;
                return;
            }
        }
        const bool visible = !g_cutscene_controls_visible.load();
        g_cutscene_controls_visible.store(visible);
        log_line("Fullscreen controls visible=%u hotkey=NumpadMultiply", visible ? 1u : 0u);
        if (!visible) save();
        dragging = -1;
        mouse_latched = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    }
    if (!g_cutscene_controls_visible.load()) return;
    RECT client{};
    POINT point{};
    if (!foreground || !GetClientRect(desc.OutputWindow, &client) ||
        client.right <= 0 || client.bottom <= 0 || !GetCursorPos(&point) ||
        !ScreenToClient(desc.OutputWindow, &point)) {
        if (dragging >= 0) save();
        dragging = -1;
        cursor = {-100, -100};
        return;
    }
    // Use the mean stereo footprint, not an obsolete full-screen rectangle.
    cursor.x = static_cast<LONG>((point.x / float(client.right) - mouse_left) /
        mouse_width * kWidth);
    cursor.y = static_cast<LONG>((point.y / float(client.bottom) - mouse_top) /
        mouse_height * kHeight);
    const bool mouse_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (mouse_down && !mouse_latched && cursor.x >= kTrackLeft - 15 &&
        cursor.x <= kTrackRight + 15) {
        for (int row = 0; row < static_cast<int>(std::size(sliders)); ++row) {
            if (abs(cursor.y - (kFirstRow + row * kRowStep)) <= 22) dragging = row;
        }
    }
    if (mouse_down && dragging >= 0) {
        auto& slider = sliders[dragging];
        const float fraction = std::clamp(
            float(cursor.x - kTrackLeft) / (kTrackRight - kTrackLeft), 0.0f, 1.0f);
        slider.value->store(slider.minimum + fraction * (slider.maximum - slider.minimum));
    }
    if (!mouse_down && mouse_latched && dragging >= 0) {
        save();
        dragging = -1;
    }
    mouse_latched = mouse_down;
}

struct Surface {
    ID3D12Resource* texture{};
    ID3D12Resource* upload{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    bool sampled{};
};
inline Surface surfaces[kXrCommandAllocatorCount];
inline ID3D12DescriptorHeap* srv_heap{};

inline void detach() {
    g_cutscene_controls_visible.store(false);
    if (game_window && previous_window_proc && IsWindow(game_window) &&
        reinterpret_cast<WNDPROC>(GetWindowLongPtrW(game_window, GWLP_WNDPROC)) ==
            window_proc) {
        SetWindowLongPtrW(game_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(previous_window_proc));
    }
    // Do not release a texture still referenced by GPU work. Process exit
    // reclaims all resources without calling possibly unloaded COM wrappers.
    if (g_xr_fence && g_xr_fence->GetCompletedValue() < g_xr_fence_value) return;
    for (auto& surface : surfaces) {
        if (surface.texture) surface.texture->Release();
        if (surface.upload) surface.upload->Release();
        surface.texture = nullptr;
        surface.upload = nullptr;
    }
    if (srv_heap) { srv_heap->Release(); srv_heap = nullptr; }
}

inline bool initialize(Surface& surface) {
    if (surface.texture && surface.upload) return true;
    if (surface.texture) { surface.texture->Release(); surface.texture = nullptr; }
    if (!srv_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC heap{};
        heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.NumDescriptors = kXrCommandAllocatorCount;
        heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(g_d3d12_device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_heap)))) return false;
    }
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC texture{};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = kWidth;
    texture.Height = kHeight;
    texture.DepthOrArraySize = 1;
    texture.MipLevels = 1;
    texture.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture.SampleDesc.Count = 1;
    if (FAILED(g_d3d12_device->CreateCommittedResource(&properties,
        D3D12_HEAP_FLAG_NONE, &texture, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&surface.texture)))) return false;
    UINT64 bytes{};
    g_d3d12_device->GetCopyableFootprints(&texture, 0, 1, 0,
        &surface.footprint, nullptr, nullptr, &bytes);
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = bytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    return SUCCEEDED(g_d3d12_device->CreateCommittedResource(&properties,
        D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&surface.upload)));
}

inline bool paint(Surface& surface) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels{};
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) { DeleteDC(dc); return false; }
    auto old_bitmap = SelectObject(dc, bitmap);
    auto fill = [dc](RECT rect, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    };
    fill({0, 0, kWidth, kHeight}, RGB(20, 25, 34));
    HFONT font = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 240, 250));
    auto text = [dc](int x, int y, const wchar_t* value) {
        TextOutW(dc, x, y, value, static_cast<int>(wcslen(value)));
    };
    text(22, 15, L"FULLSCREEN VR CONTROLS   |   Numpad * to close");
    text(22, 43, L"Drag with mouse. Positive height values LOWER the view.");
    for (int row = 0; row < static_cast<int>(std::size(sliders)); ++row) {
        const auto& slider = sliders[row];
        const float value = slider.value->load();
        const int y = kFirstRow + row * kRowStep;
        text(22, y - 11, slider.label);
        fill({kTrackLeft, y - 3, kTrackRight, y + 3}, RGB(75, 85, 100));
        const int x = kTrackLeft + static_cast<int>((value - slider.minimum) /
            (slider.maximum - slider.minimum) * (kTrackRight - kTrackLeft));
        fill({x - 7, y - 12, x + 7, y + 12}, RGB(70, 190, 250));
        wchar_t label[64]{};
        const bool scale = slider.value == &g_cinema_fullscreen_zoom ||
            slider.value == &g_cutscene_hud_size;
        swprintf_s(label, slider.degrees ? L"%.1f deg" :
            scale ? L"%.3f" : L"%.1f%%",
            slider.degrees || scale ? value : value * 100.0f);
        text(kTrackLeft, y + 16, label);
    }
    text(22, 426, L"World scale: stereo only | 0% off | 100% strong");
    fill({cursor.x - 3, cursor.y - 10, cursor.x + 3, cursor.y + 10}, RGB(255, 220, 70));
    fill({cursor.x - 10, cursor.y - 3, cursor.x + 10, cursor.y + 3}, RGB(255, 220, 70));
    GdiFlush();
    auto* color = static_cast<uint32_t*>(pixels);
    for (int i = 0; i < kWidth * kHeight; ++i) color[i] |= 0xff000000;
    void* mapped{};
    const D3D12_RANGE read_range{0, 0};
    const bool success = SUCCEEDED(surface.upload->Map(0, &read_range, &mapped));
    if (success) {
        for (int row = 0; row < kHeight; ++row) {
            memcpy(static_cast<uint8_t*>(mapped) + surface.footprint.Offset +
                row * surface.footprint.Footprint.RowPitch,
                color + row * kWidth, kWidth * sizeof(uint32_t));
        }
        surface.upload->Unmap(0, nullptr);
    }
    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return success;
}

inline void render(XrEyeSwapchain& swapchain, uint32_t image_index,
    const XrCompositionLayerProjectionView* views) {
    if (!g_cutscene_controls_visible.load() || !views || !g_xr_hud_pipeline ||
        !g_xr_hud_root_signature || !g_xr_command_list || !g_d3d12_device ||
        image_index * 2 + 1 >= swapchain.rtvs.size()) return;
    std::array<XrView, 2> panel_views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    for (uint32_t eye = 0; eye < 2; ++eye) {
        panel_views[eye].pose = views[eye].pose;
        panel_views[eye].fov = views[eye].fov;
    }
    // One coherent anatomical centre defines the panel. Each submitted eye's
    // actual pose/FOV then projects that SAME physical plane into its slice.
    // Never assign identical NDC rectangles: asymmetric/canted headsets would
    // interpret those rectangles as unrelated rays, with no finite fusion.
    std::array<XrView, 2> center_views = panel_views;
    if (g_xr_views.size() >= 2) {
        center_views[0] = g_xr_views[0];
        center_views[1] = g_xr_views[1];
    }
    w3vr::openxr_eye_geometry::EyeGeometry geometry{};
    if (!w3vr::openxr_eye_geometry::compute(center_views, geometry)) return;
    constexpr float panel_distance = 1.0f;
    constexpr float panel_width = 0.65f;
    constexpr float panel_height = panel_width * kHeight / kWidth;
    XrPosef panel_pose{};
    panel_pose.orientation = geometry.cyclopean_orientation;
    const auto offset = rotate_vector(panel_pose.orientation,
        XrVector3f{0.0f, 0.0f, -panel_distance});
    panel_pose.position = {
        geometry.cyclopean_position.x + offset.x,
        geometry.cyclopean_position.y + offset.y,
        geometry.cyclopean_position.z + offset.z};
    std::array<float, 16> panel_clip[2]{};
    float footprint_left{}, footprint_top{}, footprint_right{}, footprint_bottom{};
    for (uint32_t eye = 0; eye < 2; ++eye) {
        if (!build_anchored_panel_clip_positions(panel_views[eye], panel_pose,
                panel_width, panel_height, panel_clip[eye])) return;
        float left = 1.0e6f, right = -1.0e6f;
        float top = 1.0e6f, bottom = -1.0e6f;
        for (size_t corner = 0; corner < 4; ++corner) {
            const size_t base = corner * 4;
            const float x = (panel_clip[eye][base] / panel_clip[eye][base + 3] + 1.0f) * 0.5f;
            const float y = (1.0f - panel_clip[eye][base + 1] / panel_clip[eye][base + 3]) * 0.5f;
            left = std::min(left, x); right = std::max(right, x);
            top = std::min(top, y); bottom = std::max(bottom, y);
        }
        footprint_left += left * 0.5f; footprint_right += right * 0.5f;
        footprint_top += top * 0.5f; footprint_bottom += bottom * 0.5f;
    }
    mouse_left = footprint_left;
    mouse_top = footprint_top;
    mouse_width = std::max(footprint_right - footprint_left, 0.01f);
    mouse_height = std::max(footprint_bottom - footprint_top, 0.01f);
    // Allocator reuse is already fenced by wait_for_xr_command_allocator.
    // Texture, upload allocation and SRV use that same retirement ring.
    auto& surface = surfaces[g_xr_command_allocator_index];
    if (!initialize(surface) || !paint(surface)) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = surface.texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    if (surface.sampled) {
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        g_xr_command_list->ResourceBarrier(1, &barrier);
    }
    D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
    source.pResource = surface.upload;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = surface.footprint;
    destination.pResource = surface.texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    g_xr_command_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    g_xr_command_list->ResourceBarrier(1, &barrier);
    surface.sampled = true;
    auto cpu = srv_heap->GetCPUDescriptorHandleForHeapStart();
    auto gpu = srv_heap->GetGPUDescriptorHandleForHeapStart();
    const UINT stride = g_d3d12_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu.ptr += g_xr_command_allocator_index * stride;
    gpu.ptr += g_xr_command_allocator_index * stride;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    g_d3d12_device->CreateShaderResourceView(surface.texture, &srv, cpu);
    g_set_descriptor_heaps(g_xr_command_list, 1, &srv_heap);
    g_set_graphics_root_signature(g_xr_command_list, g_xr_hud_root_signature);
    g_set_pipeline_state(g_xr_command_list, g_xr_hud_pipeline);
    g_set_graphics_root_descriptor_table(g_xr_command_list, 0, gpu);
    g_xr_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    constexpr uint32_t sampling[8]{0, 0, 0, 0, 1, 0, 0, 0};
    g_xr_command_list->SetGraphicsRoot32BitConstants(2, 8, sampling, 0);
    for (uint32_t eye = 0; eye < 2; ++eye) {
        g_xr_command_list->SetGraphicsRoot32BitConstants(
            1, 16, panel_clip[eye].data(), 0);
        const auto& rect = views[eye].subImage.imageRect;
        D3D12_VIEWPORT viewport{float(rect.offset.x), float(rect.offset.y),
            float(rect.extent.width), float(rect.extent.height), 0, 1};
        D3D12_RECT scissor{rect.offset.x, rect.offset.y,
            rect.offset.x + rect.extent.width, rect.offset.y + rect.extent.height};
        g_xr_command_list->RSSetViewports(1, &viewport);
        g_xr_command_list->RSSetScissorRects(1, &scissor);
        const auto rtv = swapchain.rtvs[image_index * 2 + eye];
        g_xr_command_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        g_draw_instanced(g_xr_command_list, 6, 1, 0, 0);
    }
}
} // namespace cutscene_controls
