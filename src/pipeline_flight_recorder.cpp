#include "pipeline_flight_recorder.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace w3vr::pipeline_flight {
namespace {

constexpr size_t kPhaseCount = static_cast<size_t>(Phase::Count);
constexpr size_t kFrameCapacity = 4096;
constexpr size_t kGpuSampleCapacity = 1024;
constexpr size_t kQueueCapacity = 8;
constexpr uint64_t kGpuSampleStride = 4;
constexpr double kWindowSeconds = 10.0;
constexpr uint64_t kResetSequence = UINT64_MAX;

constexpr std::array<const char*, kPhaseCount> kPhaseNames{
    "frame_factory",
    "rtx_ao",
    "rtx_shadow",
    "rtx_reflection",
    "rtx_stabilize",
    "temporal_dlss",
    "temporal_taau",
    "afw",
    "xr_wait_frame",
    "xr_wait_swapchain",
    "xr_allocator_wait",
    "xr_record_composite",
    "xr_end_frame",
    "xr_frame",
    "queue_submit",
    "present"
};

struct FrameSlot {
    std::atomic<uint64_t> sequence{kResetSequence};
    std::atomic<int64_t> qpc{};
    std::atomic<int> openxr_mode{};
    std::atomic<int> temporal_backend{};
    std::atomic<uint32_t> flags{};
    std::array<std::atomic<uint64_t>, kPhaseCount> cpu_ticks{};
    std::array<std::atomic<uint32_t>, kPhaseCount> cpu_calls{};
    std::array<std::atomic<uint64_t>, kPhaseCount> gpu_ns{};
    std::array<std::atomic<uint32_t>, kPhaseCount> gpu_calls{};
};

struct FrameSnapshot {
    uint64_t sequence{};
    int64_t qpc{};
    int openxr_mode{};
    int temporal_backend{};
    uint32_t flags{};
    std::array<uint64_t, kPhaseCount> cpu_ticks{};
    std::array<uint32_t, kPhaseCount> cpu_calls{};
    std::array<uint64_t, kPhaseCount> gpu_ns{};
    std::array<uint32_t, kPhaseCount> gpu_calls{};
};

struct QueueState {
    ID3D12CommandQueue* queue{};
    ID3D12Fence* fence{};
    uint64_t frequency{};
    uint64_t next_fence{};
};

struct GpuSample {
    bool in_use{};
    bool resolved{};
    bool submitted{};
    uint32_t generation{};
    Phase phase{Phase::FrameFactory};
    uint64_t frame{};
    ID3D12CommandList* command_list{};
    QueueState* queue_state{};
    uint64_t fence_value{};
};

std::atomic<bool> g_enabled{};
std::atomic<uint64_t> g_current_frame{};
std::array<FrameSlot, kFrameCapacity> g_frames{};

std::mutex g_gpu_mutex{};
ID3D12QueryHeap* g_query_heap{};
ID3D12Resource* g_readback{};
uint64_t* g_mapped{};
bool g_gpu_initialization_failed{};
std::array<GpuSample, kGpuSampleCapacity> g_gpu_samples{};
std::array<QueueState, kQueueCapacity> g_queues{};
std::unordered_map<ID3D12CommandList*, std::vector<uint32_t>>
    g_pending_command_lists{};
uint32_t g_next_gpu_slot{};
uint32_t g_next_gpu_generation{1};
std::atomic<uint64_t> g_gpu_dropped_samples{};
std::atomic<uint64_t> g_gpu_invalid_samples{};

int64_t qpc_now() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

int64_t qpc_frequency() {
    static const int64_t value = [] {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }();
    return value;
}

FrameSlot* find_frame(uint64_t frame) {
    auto& slot = g_frames[frame % kFrameCapacity];
    return slot.sequence.load(std::memory_order_acquire) == frame
        ? &slot : nullptr;
}

void add_gpu_result(uint64_t frame, Phase phase, uint64_t nanoseconds) {
    auto* slot = find_frame(frame);
    if (slot == nullptr) {
        return;
    }
    const auto index = static_cast<size_t>(phase);
    slot->gpu_ns[index].fetch_add(nanoseconds, std::memory_order_relaxed);
    slot->gpu_calls[index].fetch_add(1, std::memory_order_relaxed);
}

bool ensure_gpu_resources(ID3D12GraphicsCommandList* command_list) {
    if (g_query_heap != nullptr && g_readback != nullptr && g_mapped != nullptr) {
        return true;
    }
    if (g_gpu_initialization_failed || command_list == nullptr) {
        return false;
    }

    ID3D12Device* device{};
    if (FAILED(command_list->GetDevice(IID_PPV_ARGS(&device))) ||
        device == nullptr) {
        g_gpu_initialization_failed = true;
        return false;
    }

    D3D12_QUERY_HEAP_DESC query_desc{};
    query_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query_desc.Count = static_cast<UINT>(kGpuSampleCapacity * 2);
    HRESULT hr = device->CreateQueryHeap(
        &query_desc, IID_PPV_ARGS(&g_query_heap));

    if (SUCCEEDED(hr)) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC resource{};
        resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource.Width = kGpuSampleCapacity * 2 * sizeof(uint64_t);
        resource.Height = 1;
        resource.DepthOrArraySize = 1;
        resource.MipLevels = 1;
        resource.SampleDesc.Count = 1;
        resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &resource,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&g_readback));
    }
    if (SUCCEEDED(hr)) {
        const D3D12_RANGE read_range{0, 0};
        hr = g_readback->Map(
            0, &read_range, reinterpret_cast<void**>(&g_mapped));
    }
    device->Release();

    if (FAILED(hr) || g_query_heap == nullptr || g_readback == nullptr ||
        g_mapped == nullptr) {
        if (g_readback != nullptr) {
            g_readback->Release();
            g_readback = nullptr;
        }
        if (g_query_heap != nullptr) {
            g_query_heap->Release();
            g_query_heap = nullptr;
        }
        g_mapped = nullptr;
        g_gpu_initialization_failed = true;
        return false;
    }
    return true;
}

QueueState* queue_state_for(ID3D12CommandQueue* queue) {
    for (auto& state : g_queues) {
        if (state.queue == queue) {
            return &state;
        }
    }
    for (auto& state : g_queues) {
        if (state.queue != nullptr) {
            continue;
        }
        uint64_t frequency{};
        if (FAILED(queue->GetTimestampFrequency(&frequency)) || frequency == 0) {
            return nullptr;
        }
        ID3D12Device* device{};
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))) || device == nullptr) {
            return nullptr;
        }
        ID3D12Fence* fence{};
        const HRESULT hr = device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        device->Release();
        if (FAILED(hr) || fence == nullptr) {
            return nullptr;
        }
        state.queue = queue;
        state.fence = fence;
        state.frequency = frequency;
        state.next_fence = 0;
        return &state;
    }
    return nullptr;
}

void release_gpu_sample(uint32_t index) {
    auto& sample = g_gpu_samples[index];
    sample = {};
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const size_t index = static_cast<size_t>(std::ceil(
        fraction * static_cast<double>(values.size()))) - 1;
    return values[std::min(index, values.size() - 1)];
}

bool flight_log_path(wchar_t (&path)[MAX_PATH]) {
    HMODULE module{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&flight_log_path), &module)) {
        return false;
    }
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash == nullptr) {
        return false;
    }
    *(slash + 1) = L'\0';
    return wcscat_s(path, L"witcher3vr_pipeline_flight.log") == 0;
}

} // namespace

void set_enabled(bool enabled_value) {
    g_enabled.store(enabled_value, std::memory_order_release);
}

bool enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

void begin_frame(
    uint64_t frame,
    int openxr_mode,
    int temporal_backend,
    bool native_asymmetric,
    bool raytracing,
    bool afw) {
    if (!enabled()) {
        return;
    }
    auto& slot = g_frames[frame % kFrameCapacity];
    slot.sequence.store(kResetSequence, std::memory_order_release);
    for (size_t phase = 0; phase < kPhaseCount; ++phase) {
        slot.cpu_ticks[phase].store(0, std::memory_order_relaxed);
        slot.cpu_calls[phase].store(0, std::memory_order_relaxed);
        slot.gpu_ns[phase].store(0, std::memory_order_relaxed);
        slot.gpu_calls[phase].store(0, std::memory_order_relaxed);
    }
    uint32_t flags{};
    if (native_asymmetric) flags |= 1u << 0;
    if (raytracing) flags |= 1u << 1;
    if (afw) flags |= 1u << 2;
    slot.qpc.store(qpc_now(), std::memory_order_relaxed);
    slot.openxr_mode.store(openxr_mode, std::memory_order_relaxed);
    slot.temporal_backend.store(temporal_backend, std::memory_order_relaxed);
    slot.flags.store(flags, std::memory_order_relaxed);
    slot.sequence.store(frame, std::memory_order_release);
    g_current_frame.store(frame, std::memory_order_release);
}

int64_t cpu_begin() {
    return enabled() ? qpc_now() : 0;
}

void cpu_end(Phase phase, int64_t begin, uint64_t frame) {
    if (begin == 0 || !enabled()) {
        return;
    }
    const int64_t end = qpc_now();
    if (end < begin) {
        return;
    }
    if (frame == UINT64_MAX) {
        frame = g_current_frame.load(std::memory_order_acquire);
    }
    auto* slot = find_frame(frame);
    if (slot == nullptr) {
        return;
    }
    const auto index = static_cast<size_t>(phase);
    slot->cpu_ticks[index].fetch_add(
        static_cast<uint64_t>(end - begin), std::memory_order_relaxed);
    slot->cpu_calls[index].fetch_add(1, std::memory_order_relaxed);
}

CpuScope::CpuScope(Phase phase, uint64_t frame)
    : phase_(phase), frame_(frame), begin_(cpu_begin()) {}

CpuScope::~CpuScope() {
    cpu_end(phase_, begin_, frame_);
}

GpuToken gpu_begin(Phase phase, ID3D12GraphicsCommandList* command_list) {
    GpuToken token{};
    if (!enabled() || command_list == nullptr) {
        return token;
    }
    const uint64_t frame = g_current_frame.load(std::memory_order_acquire);
    if (frame == 0 || frame % kGpuSampleStride != 0) {
        return token;
    }

    std::scoped_lock lock{g_gpu_mutex};
    if (!ensure_gpu_resources(command_list)) {
        g_gpu_dropped_samples.fetch_add(1, std::memory_order_relaxed);
        return token;
    }
    for (size_t attempt = 0; attempt < kGpuSampleCapacity; ++attempt) {
        const uint32_t index = static_cast<uint32_t>(
            (g_next_gpu_slot + attempt) % kGpuSampleCapacity);
        auto& sample = g_gpu_samples[index];
        if (sample.in_use) {
            continue;
        }
        g_next_gpu_slot = (index + 1) % kGpuSampleCapacity;
        sample = {};
        sample.in_use = true;
        sample.generation = g_next_gpu_generation++;
        if (sample.generation == 0) {
            sample.generation = g_next_gpu_generation++;
        }
        sample.phase = phase;
        sample.frame = frame;
        sample.command_list = command_list;
        const UINT query = index * 2;
        command_list->EndQuery(
            g_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, query);
        token.slot = index;
        token.generation = sample.generation;
        return token;
    }
    g_gpu_dropped_samples.fetch_add(1, std::memory_order_relaxed);
    return token;
}

void gpu_end(GpuToken token, ID3D12GraphicsCommandList* command_list) {
    if (token.slot >= kGpuSampleCapacity || command_list == nullptr) {
        return;
    }
    std::scoped_lock lock{g_gpu_mutex};
    auto& sample = g_gpu_samples[token.slot];
    if (!sample.in_use || sample.generation != token.generation ||
        sample.command_list != command_list || g_query_heap == nullptr ||
        g_readback == nullptr) {
        return;
    }
    const UINT query = token.slot * 2;
    command_list->EndQuery(
        g_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, query + 1);
    command_list->ResolveQueryData(
        g_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, query, 2,
        g_readback, static_cast<UINT64>(query) * sizeof(uint64_t));
    sample.resolved = true;
    g_pending_command_lists[command_list].push_back(token.slot);
}

GpuScope::GpuScope(Phase phase, ID3D12GraphicsCommandList* command_list)
    : command_list_(command_list), token_(gpu_begin(phase, command_list)) {}

GpuScope::~GpuScope() {
    gpu_end(token_, command_list_);
}

void on_execute(
    ID3D12CommandQueue* queue,
    UINT num_command_lists,
    ID3D12CommandList* const* command_lists) {
    if (!enabled() || queue == nullptr || command_lists == nullptr) {
        return;
    }
    std::scoped_lock lock{g_gpu_mutex};
    std::vector<uint32_t> submitted{};
    for (UINT index = 0; index < num_command_lists; ++index) {
        const auto found = g_pending_command_lists.find(command_lists[index]);
        if (found == g_pending_command_lists.end()) {
            continue;
        }
        submitted.insert(
            submitted.end(), found->second.begin(), found->second.end());
        g_pending_command_lists.erase(found);
    }
    if (submitted.empty()) {
        return;
    }

    QueueState* state = queue_state_for(queue);
    if (state == nullptr) {
        for (const uint32_t index : submitted) {
            release_gpu_sample(index);
        }
        g_gpu_dropped_samples.fetch_add(
            submitted.size(), std::memory_order_relaxed);
        return;
    }
    const uint64_t fence_value = ++state->next_fence;
    if (FAILED(queue->Signal(state->fence, fence_value))) {
        for (const uint32_t index : submitted) {
            release_gpu_sample(index);
        }
        g_gpu_dropped_samples.fetch_add(
            submitted.size(), std::memory_order_relaxed);
        return;
    }
    for (const uint32_t index : submitted) {
        auto& sample = g_gpu_samples[index];
        if (!sample.in_use || !sample.resolved) {
            continue;
        }
        sample.submitted = true;
        sample.queue_state = state;
        sample.fence_value = fence_value;
    }
}

void process_gpu() {
    if (!enabled()) {
        return;
    }
    std::scoped_lock lock{g_gpu_mutex};
    if (g_mapped == nullptr) {
        return;
    }
    for (uint32_t index = 0; index < kGpuSampleCapacity; ++index) {
        auto& sample = g_gpu_samples[index];
        if (!sample.in_use || !sample.resolved || !sample.submitted ||
            sample.queue_state == nullptr || sample.queue_state->fence == nullptr ||
            sample.queue_state->fence->GetCompletedValue() < sample.fence_value) {
            continue;
        }
        const uint64_t begin = g_mapped[index * 2];
        const uint64_t end = g_mapped[index * 2 + 1];
        if (end >= begin && sample.queue_state->frequency != 0) {
            const long double ns =
                static_cast<long double>(end - begin) * 1000000000.0L /
                static_cast<long double>(sample.queue_state->frequency);
            add_gpu_result(
                sample.frame, sample.phase,
                static_cast<uint64_t>(std::llround(ns)));
        } else {
            g_gpu_invalid_samples.fetch_add(1, std::memory_order_relaxed);
        }
        release_gpu_sample(index);
    }
}

bool dump_last_ten_seconds() {
    if (!enabled()) {
        return false;
    }
    process_gpu();
    const int64_t now = qpc_now();
    const int64_t frequency = qpc_frequency();
    const int64_t oldest = frequency > 0
        ? now - static_cast<int64_t>(kWindowSeconds * frequency)
        : 0;
    std::vector<FrameSnapshot> snapshots{};
    snapshots.reserve(1200);
    for (const auto& slot : g_frames) {
        const uint64_t sequence = slot.sequence.load(std::memory_order_acquire);
        if (sequence == kResetSequence) {
            continue;
        }
        FrameSnapshot snapshot{};
        snapshot.sequence = sequence;
        snapshot.qpc = slot.qpc.load(std::memory_order_relaxed);
        if (snapshot.qpc < oldest || snapshot.qpc > now) {
            continue;
        }
        snapshot.openxr_mode = slot.openxr_mode.load(std::memory_order_relaxed);
        snapshot.temporal_backend = slot.temporal_backend.load(
            std::memory_order_relaxed);
        snapshot.flags = slot.flags.load(std::memory_order_relaxed);
        for (size_t phase = 0; phase < kPhaseCount; ++phase) {
            snapshot.cpu_ticks[phase] = slot.cpu_ticks[phase].load(
                std::memory_order_relaxed);
            snapshot.cpu_calls[phase] = slot.cpu_calls[phase].load(
                std::memory_order_relaxed);
            snapshot.gpu_ns[phase] = slot.gpu_ns[phase].load(
                std::memory_order_relaxed);
            snapshot.gpu_calls[phase] = slot.gpu_calls[phase].load(
                std::memory_order_relaxed);
        }
        if (slot.sequence.load(std::memory_order_acquire) == sequence) {
            snapshots.push_back(snapshot);
        }
    }
    std::sort(snapshots.begin(), snapshots.end(),
        [](const auto& left, const auto& right) {
            return left.qpc < right.qpc;
        });

    wchar_t path[MAX_PATH]{};
    if (!flight_log_path(path)) {
        return false;
    }
    FILE* file{};
    if (_wfopen_s(&file, path, L"wb") != 0 || file == nullptr) {
        return false;
    }

    fprintf(file, "WITCHER3VR_PIPELINE_FLIGHT_V1\n");
    fprintf(file, "build=V1239 window_seconds=10 cpu_sampling=every_call "
        "gpu_sampling=every_4th_frame gpu_readback=asynchronous "
        "diagnostic_logging_required=0\n");
    fprintf(file, "frames=%zu qpc_frequency=%lld gpu_dropped=%llu "
        "gpu_invalid=%llu\n",
        snapshots.size(), static_cast<long long>(frequency),
        static_cast<unsigned long long>(
            g_gpu_dropped_samples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_gpu_invalid_samples.load(std::memory_order_relaxed)));
    fprintf(file, "backend: 0=none 1=taau 2=dlss; flags: bit0=asymmetric "
        "bit1=rtx bit2=afw\n");
    fprintf(file, "NOTE: xr_record_composite is the enclosing XR GPU list and "
        "can overlap the nested AFW value. Do not add nested phases together.\n\n");

    fprintf(file, "SUMMARY,kind,phase,active_frames,calls,avg_ms_per_active_frame,"
        "avg_ms_per_call,p95_frame_ms,p99_frame_ms,max_frame_ms\n");
    const double cpu_scale = frequency > 0
        ? 1000.0 / static_cast<double>(frequency) : 0.0;
    for (size_t phase = 0; phase < kPhaseCount; ++phase) {
        for (int kind = 0; kind < 2; ++kind) {
            std::vector<double> frame_values{};
            uint64_t calls{};
            double sum{};
            for (const auto& snapshot : snapshots) {
                const uint32_t phase_calls = kind == 0
                    ? snapshot.cpu_calls[phase] : snapshot.gpu_calls[phase];
                if (phase_calls == 0) {
                    continue;
                }
                const double milliseconds = kind == 0
                    ? static_cast<double>(snapshot.cpu_ticks[phase]) * cpu_scale
                    : static_cast<double>(snapshot.gpu_ns[phase]) / 1000000.0;
                frame_values.push_back(milliseconds);
                calls += phase_calls;
                sum += milliseconds;
            }
            if (frame_values.empty()) {
                continue;
            }
            const double maximum = *std::max_element(
                frame_values.begin(), frame_values.end());
            fprintf(file,
                "SUMMARY,%s,%s,%zu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                kind == 0 ? "cpu" : "gpu", kPhaseNames[phase],
                frame_values.size(), static_cast<unsigned long long>(calls),
                sum / static_cast<double>(frame_values.size()),
                calls != 0 ? sum / static_cast<double>(calls) : 0.0,
                percentile(frame_values, 0.95),
                percentile(frame_values, 0.99), maximum);
        }
    }

    fprintf(file, "\nFRAME,frame,age_ms,interval_ms,mode,backend,flags");
    for (const char* name : kPhaseNames) fprintf(file, ",cpu_%s_ms", name);
    for (const char* name : kPhaseNames) fprintf(file, ",gpu_%s_ms", name);
    fprintf(file, "\n");
    for (size_t index = 0; index < snapshots.size(); ++index) {
        const auto& snapshot = snapshots[index];
        const double age_ms = frequency > 0
            ? static_cast<double>(snapshot.qpc - now) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        const double interval_ms = frequency > 0 && index + 1 < snapshots.size()
            ? static_cast<double>(snapshots[index + 1].qpc - snapshot.qpc) *
                1000.0 / static_cast<double>(frequency)
            : 0.0;
        fprintf(file, "FRAME,%llu,%.3f,%.6f,%d,%d,%u",
            static_cast<unsigned long long>(snapshot.sequence), age_ms,
            interval_ms, snapshot.openxr_mode, snapshot.temporal_backend,
            snapshot.flags);
        for (size_t phase = 0; phase < kPhaseCount; ++phase) {
            fprintf(file, ",%.6f",
                static_cast<double>(snapshot.cpu_ticks[phase]) * cpu_scale);
        }
        for (size_t phase = 0; phase < kPhaseCount; ++phase) {
            if (snapshot.gpu_calls[phase] == 0) {
                fprintf(file, ",");
            } else {
                fprintf(file, ",%.6f",
                    static_cast<double>(snapshot.gpu_ns[phase]) / 1000000.0);
            }
        }
        fprintf(file, "\n");
    }
    fclose(file);
    return true;
}

} // namespace w3vr::pipeline_flight
