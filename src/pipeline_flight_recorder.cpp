#include "pipeline_flight_recorder.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace w3vr::pipeline_flight {
namespace {

constexpr size_t kPhaseCount = static_cast<size_t>(Phase::Count);
constexpr size_t kFrameCapacity = 4096;
constexpr size_t kThreadSampleCapacity = 8192;
constexpr size_t kThreadCpuIntervalCapacity = 32768;
constexpr size_t kProcessCpuIntervalCapacity = 128;
constexpr size_t kGpuSampleCapacity = 1024;
constexpr size_t kQueueCapacity = 8;
constexpr uint64_t kGpuSampleStride = 4;
constexpr uint32_t kThreadPriorityRefreshStride = 64;
constexpr DWORD kThreadCpuSamplePeriodMs = 250;
constexpr uint32_t kThreadCpuRefreshStride = 4;
constexpr double kWindowSeconds = 10.0;
constexpr uint64_t kResetSequence = UINT64_MAX;
constexpr int32_t kUnknownThreadPriority = INT32_MAX;

constexpr std::array<const char*, kPhaseCount> kPhaseNames{
    "frame_factory",
    "engine_scene_setup_hook",
    "engine_scene_setup_original",
    "engine_gameplay_hook",
    "engine_gameplay_original",
    "engine_pair_wait",
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

constexpr size_t kThreadPointCount =
    static_cast<size_t>(ThreadPoint::Count);
constexpr std::array<const char*, kThreadPointCount> kThreadPointNames{
    "producer_exit",
    "render_worker_entry"
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

struct CpuTopologyEntry {
    uint32_t cpu_set_id{};
    uint16_t group{};
    uint16_t logical_processor{};
    int32_t core_index{-1};
    int32_t efficiency_class{-1};
    int32_t scheduling_class{-1};
    uint32_t flags{};
};

struct ThreadSampleSlot {
    std::atomic<uint64_t> sequence{kResetSequence};
    std::atomic<int64_t> qpc{};
    std::atomic<uint64_t> frame{};
    std::atomic<uint64_t> pair_id{};
    std::atomic<uint32_t> point{};
    std::atomic<int32_t> eye{-1};
    std::atomic<uint32_t> thread_id{};
    std::atomic<uint32_t> group{};
    std::atomic<uint32_t> logical_processor{};
    std::atomic<int32_t> core_index{-1};
    std::atomic<int32_t> efficiency_class{-1};
    std::atomic<int32_t> scheduling_class{-1};
    std::atomic<int32_t> thread_priority{kUnknownThreadPriority};
};

struct ThreadSampleSnapshot {
    uint64_t sequence{};
    int64_t qpc{};
    uint64_t frame{};
    uint64_t pair_id{};
    uint32_t point{};
    int32_t eye{-1};
    uint32_t thread_id{};
    uint32_t group{};
    uint32_t logical_processor{};
    int32_t core_index{-1};
    int32_t efficiency_class{-1};
    int32_t scheduling_class{-1};
    int32_t thread_priority{kUnknownThreadPriority};
};

struct ProcessThreadSnapshot {
    uint32_t thread_id{};
    int32_t base_priority{};
    int32_t delta_priority{};
    int32_t relative_priority{kUnknownThreadPriority};
    int32_t priority_boost_disabled{-1};
    int32_t affinity_group{-1};
    uint64_t affinity_mask{};
    int32_t ideal_group{-1};
    int32_t ideal_processor{-1};
    std::string selected_cpu_sets{};
    std::string description{};
};

struct ThreadCpuInterval {
    int64_t begin_qpc{};
    int64_t end_qpc{};
    uint32_t thread_id{};
    uint64_t kernel_100ns{};
    uint64_t user_100ns{};
    uint64_t cycles{};
};

struct ProcessCpuInterval {
    int64_t begin_qpc{};
    int64_t end_qpc{};
    uint64_t kernel_100ns{};
    uint64_t user_100ns{};
    uint32_t queried_threads{};
    uint32_t failed_threads{};
    uint64_t sampler_wall_ticks{};
};

struct LiveThreadCounter {
    HANDLE handle{};
    uint64_t kernel_100ns{};
    uint64_t user_100ns{};
    uint64_t cycles{};
    int64_t qpc{};
    uint32_t refresh_generation{};
    bool initialized{};
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
std::atomic<uint64_t> g_thread_sample_sequence{};
std::array<ThreadSampleSlot, kThreadSampleCapacity> g_thread_samples{};
std::once_flag g_cpu_topology_once{};
std::vector<CpuTopologyEntry> g_cpu_topology{};

std::mutex g_thread_cpu_mutex{};
std::array<ThreadCpuInterval, kThreadCpuIntervalCapacity>
    g_thread_cpu_intervals{};
size_t g_thread_cpu_next{};
size_t g_thread_cpu_count{};
std::array<ProcessCpuInterval, kProcessCpuIntervalCapacity>
    g_process_cpu_intervals{};
size_t g_process_cpu_next{};
size_t g_process_cpu_count{};
std::mutex g_thread_cpu_lifecycle_mutex{};
HANDLE g_thread_cpu_stop_event{};
HANDLE g_thread_cpu_sampler_thread{};
std::atomic<uint32_t> g_thread_cpu_sampler_tid{};
std::atomic<uint64_t> g_thread_cpu_samples_taken{};
std::atomic<uint64_t> g_thread_cpu_query_failures{};
std::atomic<uint64_t> g_dump_sequence{};

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

void initialize_cpu_topology() {
    std::call_once(g_cpu_topology_once, []() {
        ULONG required_bytes{};
        GetSystemCpuSetInformation(
            nullptr, 0, &required_bytes, GetCurrentProcess(), 0);
        if (required_bytes < sizeof(SYSTEM_CPU_SET_INFORMATION)) {
            return;
        }

        std::vector<uint8_t> buffer(required_bytes);
        if (!GetSystemCpuSetInformation(
                reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()),
                required_bytes, &required_bytes, GetCurrentProcess(), 0)) {
            return;
        }

        size_t offset{};
        while (offset + sizeof(ULONG) <= required_bytes) {
            const auto* entry =
                reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(
                    buffer.data() + offset);
            if (entry->Size < sizeof(ULONG) ||
                offset + entry->Size > required_bytes) {
                break;
            }
            if (entry->Type == CpuSetInformation) {
                CpuTopologyEntry topology{};
                topology.cpu_set_id = entry->CpuSet.Id;
                topology.group = entry->CpuSet.Group;
                topology.logical_processor =
                    entry->CpuSet.LogicalProcessorIndex;
                topology.core_index = entry->CpuSet.CoreIndex;
                topology.efficiency_class = entry->CpuSet.EfficiencyClass;
                topology.scheduling_class = entry->CpuSet.SchedulingClass;
                topology.flags = entry->CpuSet.AllFlags;
                g_cpu_topology.push_back(topology);
            }
            offset += entry->Size;
        }
        std::sort(g_cpu_topology.begin(), g_cpu_topology.end(),
            [](const auto& left, const auto& right) {
                return std::tie(left.group, left.logical_processor) <
                    std::tie(right.group, right.logical_processor);
            });
    });
}

const CpuTopologyEntry* find_cpu_topology(
    uint16_t group,
    uint16_t logical_processor) {
    const auto found = std::lower_bound(
        g_cpu_topology.begin(), g_cpu_topology.end(),
        std::pair{group, logical_processor},
        [](const CpuTopologyEntry& entry, const auto& key) {
            return entry.group < key.first ||
                (entry.group == key.first &&
                    entry.logical_processor < key.second);
        });
    return found != g_cpu_topology.end() && found->group == group &&
            found->logical_processor == logical_processor
        ? &*found
        : nullptr;
}

const char* thread_point_name(uint32_t point) {
    return point < kThreadPointCount
        ? kThreadPointNames[point]
        : "unknown";
}

std::string csv_safe_utf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') {
        return {};
    }
    const int bytes = WideCharToMultiByte(
        CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text, -1, result.data(), bytes, nullptr, nullptr);
    result.pop_back();
    for (char& character : result) {
        if (character == ',' || character == '\r' || character == '\n') {
            character = ' ';
        }
    }
    return result;
}

std::vector<ProcessThreadSnapshot> snapshot_process_threads() {
    std::vector<ProcessThreadSnapshot> snapshots{};
    const DWORD process_id = GetCurrentProcessId();
    const HANDLE toolhelp = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (toolhelp == INVALID_HANDLE_VALUE) {
        return snapshots;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(toolhelp, &entry)) {
        do {
            if (entry.th32OwnerProcessID != process_id) {
                continue;
            }
            ProcessThreadSnapshot snapshot{};
            snapshot.thread_id = entry.th32ThreadID;
            snapshot.base_priority = entry.tpBasePri;
            snapshot.delta_priority = entry.tpDeltaPri;

            const HANDLE thread = OpenThread(
                THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                entry.th32ThreadID);
            if (thread != nullptr) {
                snapshot.relative_priority = GetThreadPriority(thread);
                BOOL boost_disabled{};
                if (GetThreadPriorityBoost(thread, &boost_disabled)) {
                    snapshot.priority_boost_disabled = boost_disabled ? 1 : 0;
                }
                GROUP_AFFINITY affinity{};
                if (GetThreadGroupAffinity(thread, &affinity)) {
                    snapshot.affinity_group = affinity.Group;
                    snapshot.affinity_mask = affinity.Mask;
                }
                PROCESSOR_NUMBER ideal{};
                if (GetThreadIdealProcessorEx(thread, &ideal)) {
                    snapshot.ideal_group = ideal.Group;
                    snapshot.ideal_processor = ideal.Number;
                }

                ULONG required_cpu_sets{};
                GetThreadSelectedCpuSets(
                    thread, nullptr, 0, &required_cpu_sets);
                if (required_cpu_sets != 0) {
                    std::vector<ULONG> cpu_sets(required_cpu_sets);
                    if (GetThreadSelectedCpuSets(
                            thread, cpu_sets.data(),
                            static_cast<ULONG>(cpu_sets.size()),
                            &required_cpu_sets)) {
                        std::ostringstream selected{};
                        for (size_t index = 0; index < required_cpu_sets;
                             ++index) {
                            if (index != 0) {
                                selected << ';';
                            }
                            selected << cpu_sets[index];
                        }
                        snapshot.selected_cpu_sets = selected.str();
                    }
                }

                PWSTR description{};
                if (SUCCEEDED(GetThreadDescription(thread, &description)) &&
                    description != nullptr) {
                    snapshot.description = csv_safe_utf8(description);
                    LocalFree(description);
                }
                CloseHandle(thread);
            }
            snapshots.push_back(std::move(snapshot));
        } while (Thread32Next(toolhelp, &entry));
    }
    CloseHandle(toolhelp);
    std::sort(snapshots.begin(), snapshots.end(),
        [](const auto& left, const auto& right) {
            return left.thread_id < right.thread_id;
        });
    return snapshots;
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

uint64_t filetime_ticks(const FILETIME& value) {
    ULARGE_INTEGER ticks{};
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return ticks.QuadPart;
}

void append_thread_cpu_intervals(
    const std::vector<ThreadCpuInterval>& thread_intervals,
    const ProcessCpuInterval* process_interval) {
    std::scoped_lock lock{g_thread_cpu_mutex};
    for (const auto& interval : thread_intervals) {
        g_thread_cpu_intervals[g_thread_cpu_next] = interval;
        g_thread_cpu_next =
            (g_thread_cpu_next + 1) % kThreadCpuIntervalCapacity;
        g_thread_cpu_count = std::min(
            g_thread_cpu_count + 1, kThreadCpuIntervalCapacity);
    }
    if (process_interval != nullptr) {
        g_process_cpu_intervals[g_process_cpu_next] = *process_interval;
        g_process_cpu_next =
            (g_process_cpu_next + 1) % kProcessCpuIntervalCapacity;
        g_process_cpu_count = std::min(
            g_process_cpu_count + 1, kProcessCpuIntervalCapacity);
    }
}

void refresh_live_thread_counters(
    std::unordered_map<uint32_t, LiveThreadCounter>& counters,
    uint32_t generation) {
    const DWORD process_id = GetCurrentProcessId();
    const DWORD sampler_tid = GetCurrentThreadId();
    const HANDLE toolhelp = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (toolhelp == INVALID_HANDLE_VALUE) {
        return;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(toolhelp, &entry)) {
        do {
            if (entry.th32OwnerProcessID != process_id ||
                entry.th32ThreadID == sampler_tid) {
                continue;
            }
            const uint32_t thread_id = entry.th32ThreadID;
            auto found = counters.find(thread_id);
            if (found == counters.end()) {
                const HANDLE thread = OpenThread(
                    THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                    FALSE, thread_id);
                if (thread == nullptr) {
                    g_thread_cpu_query_failures.fetch_add(
                        1, std::memory_order_relaxed);
                    continue;
                }
                LiveThreadCounter counter{};
                counter.handle = thread;
                counter.refresh_generation = generation;
                counters.emplace(thread_id, counter);
            } else {
                found->second.refresh_generation = generation;
            }
        } while (Thread32Next(toolhelp, &entry));
    }
    CloseHandle(toolhelp);

    for (auto found = counters.begin(); found != counters.end();) {
        if (found->second.refresh_generation == generation) {
            ++found;
            continue;
        }
        CloseHandle(found->second.handle);
        found = counters.erase(found);
    }
}

DWORD WINAPI thread_cpu_sampler_main(void*) {
    g_thread_cpu_sampler_tid.store(
        GetCurrentThreadId(), std::memory_order_release);
    SetThreadDescription(GetCurrentThread(), L"W3VR Flight CPU Sampler");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    std::unordered_map<uint32_t, LiveThreadCounter> counters{};
    uint32_t refresh_generation{};
    uint32_t sample_index{};
    uint64_t previous_process_kernel{};
    uint64_t previous_process_user{};
    int64_t previous_process_qpc{};
    bool process_initialized{};

    while (g_enabled.load(std::memory_order_acquire)) {
        const int64_t sampler_begin = qpc_now();
        if (sample_index % kThreadCpuRefreshStride == 0) {
            ++refresh_generation;
            refresh_live_thread_counters(counters, refresh_generation);
        }

        std::vector<ThreadCpuInterval> intervals{};
        intervals.reserve(counters.size());
        uint32_t queried_threads{};
        uint32_t failed_threads{};
        for (auto& [thread_id, counter] : counters) {
            FILETIME creation{}, exit{}, kernel{}, user{};
            ULONG64 cycles{};
            const bool time_ok = GetThreadTimes(
                counter.handle, &creation, &exit, &kernel, &user) != FALSE;
            const bool cycles_ok =
                QueryThreadCycleTime(counter.handle, &cycles) != FALSE;
            if (!time_ok || !cycles_ok) {
                ++failed_threads;
                g_thread_cpu_query_failures.fetch_add(
                    1, std::memory_order_relaxed);
                continue;
            }
            ++queried_threads;
            const uint64_t kernel_ticks = filetime_ticks(kernel);
            const uint64_t user_ticks = filetime_ticks(user);
            const int64_t sample_qpc = qpc_now();
            if (counter.initialized && sample_qpc >= counter.qpc &&
                kernel_ticks >= counter.kernel_100ns &&
                user_ticks >= counter.user_100ns &&
                cycles >= counter.cycles) {
                intervals.push_back(ThreadCpuInterval{
                    counter.qpc,
                    sample_qpc,
                    thread_id,
                    kernel_ticks - counter.kernel_100ns,
                    user_ticks - counter.user_100ns,
                    cycles - counter.cycles});
            }
            counter.kernel_100ns = kernel_ticks;
            counter.user_100ns = user_ticks;
            counter.cycles = cycles;
            counter.qpc = sample_qpc;
            counter.initialized = true;
        }

        ProcessCpuInterval process_interval{};
        ProcessCpuInterval* process_interval_ptr{};
        FILETIME process_creation{}, process_exit{}, process_kernel{},
            process_user{};
        const int64_t process_qpc = qpc_now();
        if (GetProcessTimes(GetCurrentProcess(), &process_creation,
                &process_exit, &process_kernel, &process_user)) {
            const uint64_t kernel_ticks = filetime_ticks(process_kernel);
            const uint64_t user_ticks = filetime_ticks(process_user);
            if (process_initialized && process_qpc >= previous_process_qpc &&
                kernel_ticks >= previous_process_kernel &&
                user_ticks >= previous_process_user) {
                process_interval.begin_qpc = previous_process_qpc;
                process_interval.end_qpc = process_qpc;
                process_interval.kernel_100ns =
                    kernel_ticks - previous_process_kernel;
                process_interval.user_100ns = user_ticks - previous_process_user;
                process_interval.queried_threads = queried_threads;
                process_interval.failed_threads = failed_threads;
                process_interval_ptr = &process_interval;
            }
            previous_process_kernel = kernel_ticks;
            previous_process_user = user_ticks;
            previous_process_qpc = process_qpc;
            process_initialized = true;
        }

        const int64_t sampler_end = qpc_now();
        if (process_interval_ptr != nullptr && sampler_end >= sampler_begin) {
            process_interval.sampler_wall_ticks =
                static_cast<uint64_t>(sampler_end - sampler_begin);
        }
        append_thread_cpu_intervals(intervals, process_interval_ptr);
        g_thread_cpu_samples_taken.fetch_add(1, std::memory_order_relaxed);
        ++sample_index;

        const HANDLE stop_event = g_thread_cpu_stop_event;
        if (stop_event == nullptr ||
            WaitForSingleObject(stop_event, kThreadCpuSamplePeriodMs) !=
                WAIT_TIMEOUT) {
            break;
        }
    }

    for (auto& [thread_id, counter] : counters) {
        (void)thread_id;
        CloseHandle(counter.handle);
    }
    g_thread_cpu_sampler_tid.store(0, std::memory_order_release);
    return 0;
}

void start_thread_cpu_sampler() {
    std::scoped_lock lock{g_thread_cpu_lifecycle_mutex};
    if (g_thread_cpu_sampler_thread != nullptr) {
        return;
    }
    g_thread_cpu_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_thread_cpu_stop_event == nullptr) {
        return;
    }
    g_thread_cpu_sampler_thread = CreateThread(
        nullptr, 0, &thread_cpu_sampler_main, nullptr, 0, nullptr);
    if (g_thread_cpu_sampler_thread == nullptr) {
        CloseHandle(g_thread_cpu_stop_event);
        g_thread_cpu_stop_event = nullptr;
    }
}

void stop_thread_cpu_sampler() {
    std::scoped_lock lock{g_thread_cpu_lifecycle_mutex};
    if (g_thread_cpu_sampler_thread == nullptr) {
        return;
    }
    SetEvent(g_thread_cpu_stop_event);
    WaitForSingleObject(g_thread_cpu_sampler_thread, 2000);
    CloseHandle(g_thread_cpu_sampler_thread);
    CloseHandle(g_thread_cpu_stop_event);
    g_thread_cpu_sampler_thread = nullptr;
    g_thread_cpu_stop_event = nullptr;
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

bool open_unique_flight_log(
    FILE** file,
    wchar_t (&path)[MAX_PATH]) {
    if (file == nullptr) {
        return false;
    }
    *file = nullptr;
    HMODULE module{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&open_unique_flight_log), &module)) {
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
    const size_t directory_length = wcslen(path);
    SYSTEMTIME local_time{};
    GetLocalTime(&local_time);
    for (uint32_t attempt = 0; attempt < 100; ++attempt) {
        const uint64_t sequence =
            g_dump_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        const int written = swprintf_s(
            path + directory_length,
            MAX_PATH - directory_length,
            L"witcher3vr_pipeline_flight_%04u%02u%02u_%02u%02u%02u_%03u_"
            L"pid%lu_%06llu.log",
            local_time.wYear, local_time.wMonth, local_time.wDay,
            local_time.wHour, local_time.wMinute, local_time.wSecond,
            local_time.wMilliseconds,
            static_cast<unsigned long>(GetCurrentProcessId()),
            static_cast<unsigned long long>(sequence));
        if (written <= 0) {
            return false;
        }

        const HANDLE handle = CreateFileW(
            path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS ||
                GetLastError() == ERROR_ALREADY_EXISTS) {
                continue;
            }
            return false;
        }
        const int descriptor = _open_osfhandle(
            reinterpret_cast<intptr_t>(handle),
            _O_BINARY | _O_WRONLY | _O_NOINHERIT);
        if (descriptor == -1) {
            CloseHandle(handle);
            return false;
        }
        FILE* stream = _fdopen(descriptor, "wb");
        if (stream == nullptr) {
            _close(descriptor);
            return false;
        }
        *file = stream;
        return true;
    }
    return false;
}

} // namespace

void set_enabled(bool enabled_value) {
    if (enabled_value) {
        initialize_cpu_topology();
        g_enabled.store(true, std::memory_order_release);
        start_thread_cpu_sampler();
        return;
    }
    g_enabled.store(false, std::memory_order_release);
    stop_thread_cpu_sampler();
}

bool enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

uint64_t current_frame() {
    return g_current_frame.load(std::memory_order_acquire);
}

void begin_frame(
    uint64_t frame,
    int openxr_mode,
    int temporal_backend,
    bool native_asymmetric,
    bool raytracing,
    bool afw,
    bool afw_visual_debug) {
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
    if (afw_visual_debug) flags |= 1u << 3;
    slot.qpc.store(qpc_now(), std::memory_order_relaxed);
    slot.openxr_mode.store(openxr_mode, std::memory_order_relaxed);
    slot.temporal_backend.store(temporal_backend, std::memory_order_relaxed);
    slot.flags.store(flags, std::memory_order_relaxed);
    slot.sequence.store(frame, std::memory_order_release);
    g_current_frame.store(frame, std::memory_order_release);
}

void record_thread_sample(
    ThreadPoint point,
    uint64_t pair_id,
    int eye) {
    if (!enabled() ||
        static_cast<uint32_t>(point) >=
            static_cast<uint32_t>(ThreadPoint::Count)) {
        return;
    }

    PROCESSOR_NUMBER processor{};
    GetCurrentProcessorNumberEx(&processor);
    const DWORD thread_id = GetCurrentThreadId();

    // GetThreadPriority is deliberately not called on every frame. REDengine
    // threads are long-lived, while refreshing every 64 observations still
    // catches configuration changes inside a ten-second flight window.
    struct PriorityCache {
        DWORD thread_id{};
        uint32_t samples{};
        int32_t priority{kUnknownThreadPriority};
    };
    thread_local PriorityCache priority_cache{};
    if (priority_cache.thread_id != thread_id ||
        priority_cache.samples % kThreadPriorityRefreshStride == 0) {
        priority_cache.thread_id = thread_id;
        priority_cache.priority = GetThreadPriority(GetCurrentThread());
    }
    ++priority_cache.samples;

    int32_t core_index{-1};
    int32_t efficiency_class{-1};
    int32_t scheduling_class{-1};
    if (const auto* topology = find_cpu_topology(
            processor.Group, processor.Number)) {
        core_index = topology->core_index;
        efficiency_class = topology->efficiency_class;
        scheduling_class = topology->scheduling_class;
    }

    const uint64_t sequence = g_thread_sample_sequence.fetch_add(
        1, std::memory_order_relaxed);
    auto& slot = g_thread_samples[sequence % kThreadSampleCapacity];
    slot.sequence.store(kResetSequence, std::memory_order_release);
    slot.qpc.store(qpc_now(), std::memory_order_relaxed);
    slot.frame.store(
        g_current_frame.load(std::memory_order_acquire),
        std::memory_order_relaxed);
    slot.pair_id.store(pair_id, std::memory_order_relaxed);
    slot.point.store(static_cast<uint32_t>(point), std::memory_order_relaxed);
    slot.eye.store(eye, std::memory_order_relaxed);
    slot.thread_id.store(thread_id, std::memory_order_relaxed);
    slot.group.store(processor.Group, std::memory_order_relaxed);
    slot.logical_processor.store(processor.Number, std::memory_order_relaxed);
    slot.core_index.store(core_index, std::memory_order_relaxed);
    slot.efficiency_class.store(efficiency_class, std::memory_order_relaxed);
    slot.scheduling_class.store(scheduling_class, std::memory_order_relaxed);
    slot.thread_priority.store(
        priority_cache.priority, std::memory_order_relaxed);
    slot.sequence.store(sequence, std::memory_order_release);
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

    std::vector<ThreadSampleSnapshot> thread_snapshots{};
    thread_snapshots.reserve(4096);
    for (const auto& slot : g_thread_samples) {
        const uint64_t sequence = slot.sequence.load(std::memory_order_acquire);
        if (sequence == kResetSequence) {
            continue;
        }
        ThreadSampleSnapshot snapshot{};
        snapshot.sequence = sequence;
        snapshot.qpc = slot.qpc.load(std::memory_order_relaxed);
        if (snapshot.qpc < oldest || snapshot.qpc > now) {
            continue;
        }
        snapshot.frame = slot.frame.load(std::memory_order_relaxed);
        snapshot.pair_id = slot.pair_id.load(std::memory_order_relaxed);
        snapshot.point = slot.point.load(std::memory_order_relaxed);
        snapshot.eye = slot.eye.load(std::memory_order_relaxed);
        snapshot.thread_id = slot.thread_id.load(std::memory_order_relaxed);
        snapshot.group = slot.group.load(std::memory_order_relaxed);
        snapshot.logical_processor = slot.logical_processor.load(
            std::memory_order_relaxed);
        snapshot.core_index = slot.core_index.load(std::memory_order_relaxed);
        snapshot.efficiency_class = slot.efficiency_class.load(
            std::memory_order_relaxed);
        snapshot.scheduling_class = slot.scheduling_class.load(
            std::memory_order_relaxed);
        snapshot.thread_priority = slot.thread_priority.load(
            std::memory_order_relaxed);
        if (slot.sequence.load(std::memory_order_acquire) == sequence) {
            thread_snapshots.push_back(snapshot);
        }
    }
    std::sort(thread_snapshots.begin(), thread_snapshots.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.qpc, left.sequence) <
                std::tie(right.qpc, right.sequence);
        });

    std::vector<ThreadCpuInterval> thread_cpu_intervals{};
    std::vector<ProcessCpuInterval> process_cpu_intervals{};
    {
        std::scoped_lock lock{g_thread_cpu_mutex};
        thread_cpu_intervals.reserve(g_thread_cpu_count);
        const size_t thread_begin =
            (g_thread_cpu_next + kThreadCpuIntervalCapacity -
                g_thread_cpu_count) % kThreadCpuIntervalCapacity;
        for (size_t index = 0; index < g_thread_cpu_count; ++index) {
            const auto& interval = g_thread_cpu_intervals[
                (thread_begin + index) % kThreadCpuIntervalCapacity];
            if (interval.end_qpc >= oldest && interval.end_qpc <= now) {
                thread_cpu_intervals.push_back(interval);
            }
        }

        process_cpu_intervals.reserve(g_process_cpu_count);
        const size_t process_begin =
            (g_process_cpu_next + kProcessCpuIntervalCapacity -
                g_process_cpu_count) % kProcessCpuIntervalCapacity;
        for (size_t index = 0; index < g_process_cpu_count; ++index) {
            const auto& interval = g_process_cpu_intervals[
                (process_begin + index) % kProcessCpuIntervalCapacity];
            if (interval.end_qpc >= oldest && interval.end_qpc <= now) {
                process_cpu_intervals.push_back(interval);
            }
        }
    }
    std::sort(thread_cpu_intervals.begin(), thread_cpu_intervals.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.end_qpc, left.thread_id) <
                std::tie(right.end_qpc, right.thread_id);
        });
    std::sort(process_cpu_intervals.begin(), process_cpu_intervals.end(),
        [](const auto& left, const auto& right) {
            return left.end_qpc < right.end_qpc;
        });
    const auto process_threads = snapshot_process_threads();

    wchar_t path[MAX_PATH]{};
    FILE* file{};
    if (!open_unique_flight_log(&file, path) || file == nullptr) {
        return false;
    }

    fprintf(file, "WITCHER3VR_PIPELINE_FLIGHT_V3\n");
    fprintf(file, "build=V1255 base=V1254 window_seconds=10 "
        "cpu_sampling=every_call "
        "gpu_sampling=every_4th_frame gpu_readback=asynchronous "
        "thread_sampling=producer_exit+render_worker_entry "
        "thread_priority_refresh=64_samples thread_cpu_period_ms=%lu "
        "thread_cpu_source=GetThreadTimes+QueryThreadCycleTime "
        "diagnostic_logging_required=0\n",
        static_cast<unsigned long>(kThreadCpuSamplePeriodMs));
    fprintf(file, "file_policy=timestamped_create_new_no_overwrite path=%ls\n",
        path);
    fprintf(file, "frames=%zu thread_samples=%zu thread_cpu_intervals=%zu "
        "process_cpu_intervals=%zu process_threads=%zu "
        "thread_capacity=%zu thread_total=%llu qpc_frequency=%lld "
        "thread_cpu_sampler_tid=%u thread_cpu_sampler_passes=%llu "
        "thread_cpu_query_failures=%llu gpu_dropped=%llu gpu_invalid=%llu\n",
        snapshots.size(), thread_snapshots.size(), thread_cpu_intervals.size(),
        process_cpu_intervals.size(), process_threads.size(),
        kThreadSampleCapacity,
        static_cast<unsigned long long>(
            g_thread_sample_sequence.load(std::memory_order_relaxed)),
        static_cast<long long>(frequency),
        g_thread_cpu_sampler_tid.load(std::memory_order_acquire),
        static_cast<unsigned long long>(
            g_thread_cpu_samples_taken.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_thread_cpu_query_failures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_gpu_dropped_samples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_gpu_invalid_samples.load(std::memory_order_relaxed)));
    fprintf(file, "backend: 0=none 1=taau 2=dlss; flags: bit0=asymmetric "
        "bit1=rtx bit2=afw bit3=afw_visual_debug\n");
    fprintf(file, "NOTE: xr_record_composite is the enclosing XR GPU list and "
        "can overlap the nested AFW value. Do not add nested phases together.\n\n");
    fprintf(file, "NOTE: engine_*_hook includes its nested engine_*_original "
        "phase (and engine_pair_wait where applicable). Do not add those "
        "nested CPU phases together.\n");
    fprintf(file, "NOTE: THREAD_CPU measures CPU actually consumed by each "
        "TID. A long wall interval with little CPU is evidence of waiting or "
        "lack of scheduling, but this recorder does not directly capture "
        "Windows ready-queue time; ETW is required for that distinction.\n");
    fprintf(file, "NOTE: the dedicated sampler runs at THREAD_PRIORITY_LOWEST, "
        "never changes game-thread affinity/priority, and is excluded from "
        "THREAD_CPU rows. sampler_wall_ms reports each sampling pass cost.\n");
    fprintf(file, "NOTE: THREAD rows are exact samples only for the inline "
        "producer/render-worker hooks, not the complete REDengine worker pool. "
        "Use ETW for whole-process scheduling.\n");
    fprintf(file, "NOTE: thread_priority is the configured Win32 relative "
        "priority returned by GetThreadPriority, not the scheduler's dynamic "
        "boosted priority; 2147483647 means unavailable.\n");
    fprintf(file, "NOTE: PROCESS_THREAD_CONFIG is a dump-time policy snapshot "
        "for every process thread (affinity/ideal CPU/selected CPU sets), not "
        "a sample of the CPU where each thread was running.\n");
    fprintf(file, "NOTE: core_index is topology-local to processor_group; SMT "
        "siblings share group+core_index.\n\n");

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

    using PlacementKey = std::tuple<
        uint32_t, uint32_t, uint32_t, uint32_t,
        int32_t, int32_t, int32_t, int32_t>;
    std::map<PlacementKey, size_t> placement_counts{};
    std::array<size_t, kThreadPointCount> point_totals{};
    std::array<std::set<uint32_t>, kThreadPointCount> point_threads{};
    std::array<std::set<uint64_t>, kThreadPointCount> point_logical_cpus{};
    std::array<std::set<uint64_t>, kThreadPointCount> point_physical_cores{};
    std::array<size_t, kThreadPointCount> logical_migrations{};
    std::array<size_t, kThreadPointCount> physical_migrations{};
    using ThreadIdentity = std::pair<uint32_t, uint32_t>;
    using ThreadLocation = std::tuple<uint32_t, uint32_t, int32_t>;
    std::map<ThreadIdentity, ThreadLocation> previous_locations{};
    for (const auto& sample : thread_snapshots) {
        if (sample.point >= kThreadPointCount) {
            continue;
        }
        const size_t point = sample.point;
        ++point_totals[point];
        point_threads[point].insert(sample.thread_id);
        point_logical_cpus[point].insert(
            (static_cast<uint64_t>(sample.group) << 32) |
            sample.logical_processor);
        if (sample.core_index >= 0) {
            point_physical_cores[point].insert(
                (static_cast<uint64_t>(sample.group) << 32) |
                static_cast<uint32_t>(sample.core_index));
        }
        ++placement_counts[PlacementKey{
            sample.point, sample.thread_id, sample.group,
            sample.logical_processor, sample.core_index,
            sample.efficiency_class, sample.scheduling_class,
            sample.thread_priority}];

        const ThreadIdentity identity{sample.point, sample.thread_id};
        const ThreadLocation location{
            sample.group, sample.logical_processor, sample.core_index};
        const auto previous = previous_locations.find(identity);
        if (previous != previous_locations.end()) {
            const auto& [old_group, old_logical, old_core] = previous->second;
            if (old_group != sample.group ||
                old_logical != sample.logical_processor) {
                ++logical_migrations[point];
            }
            if (old_core >= 0 && sample.core_index >= 0 &&
                (old_group != sample.group || old_core != sample.core_index)) {
                ++physical_migrations[point];
            }
        }
        previous_locations[identity] = location;
    }

    fprintf(file, "\nTHREAD_ROLE_SUMMARY,point,samples,unique_tids,"
        "unique_logical_cpus,unique_physical_cores,logical_migrations,"
        "physical_migrations\n");
    for (size_t point = 0; point < kThreadPointCount; ++point) {
        if (point_totals[point] == 0) {
            continue;
        }
        fprintf(file, "THREAD_ROLE_SUMMARY,%s,%zu,%zu,%zu,%zu,%zu,%zu\n",
            kThreadPointNames[point], point_totals[point],
            point_threads[point].size(), point_logical_cpus[point].size(),
            point_physical_cores[point].size(), logical_migrations[point],
            physical_migrations[point]);
    }

    fprintf(file, "\nTHREAD_PLACEMENT_SUMMARY,point,tid,group,logical_cpu,"
        "core_index,efficiency_class,scheduling_class,thread_priority,samples,"
        "share_of_point_pct\n");
    for (const auto& [key, count] : placement_counts) {
        const auto& [point, thread_id, group, logical_processor, core_index,
            efficiency_class, scheduling_class, thread_priority] = key;
        const double share = point < kThreadPointCount && point_totals[point] != 0
            ? static_cast<double>(count) * 100.0 /
                static_cast<double>(point_totals[point])
            : 0.0;
        fprintf(file,
            "THREAD_PLACEMENT_SUMMARY,%s,%u,%u,%u,%d,%d,%d,%d,%zu,%.3f\n",
            thread_point_name(point), thread_id, group, logical_processor,
            core_index, efficiency_class, scheduling_class, thread_priority,
            count, share);
    }

    fprintf(file, "\nPROCESS_THREAD_CONFIG,tid,base_priority,delta_priority,"
        "relative_priority,priority_boost_disabled,affinity_group,"
        "affinity_mask,ideal_group,ideal_cpu,selected_cpu_sets,name\n");
    for (const auto& thread : process_threads) {
        fprintf(file,
            "PROCESS_THREAD_CONFIG,%u,%d,%d,%d,%d,%d,0x%016llX,%d,%d,%s,%s\n",
            thread.thread_id, thread.base_priority, thread.delta_priority,
            thread.relative_priority, thread.priority_boost_disabled,
            thread.affinity_group,
            static_cast<unsigned long long>(thread.affinity_mask),
            thread.ideal_group, thread.ideal_processor,
            thread.selected_cpu_sets.c_str(), thread.description.c_str());
    }

    struct ThreadCpuSummary {
        uint64_t kernel_100ns{};
        uint64_t user_100ns{};
        uint64_t cycles{};
        uint64_t observed_qpc{};
        size_t samples{};
        size_t active_samples{};
        std::vector<double> sample_cpu_ms{};
    };
    std::map<uint32_t, ThreadCpuSummary> thread_cpu_summaries{};
    for (const auto& interval : thread_cpu_intervals) {
        auto& summary = thread_cpu_summaries[interval.thread_id];
        summary.kernel_100ns += interval.kernel_100ns;
        summary.user_100ns += interval.user_100ns;
        summary.cycles += interval.cycles;
        if (interval.end_qpc >= interval.begin_qpc) {
            summary.observed_qpc += static_cast<uint64_t>(
                interval.end_qpc - interval.begin_qpc);
        }
        ++summary.samples;
        const uint64_t cpu_100ns =
            interval.kernel_100ns + interval.user_100ns;
        if (cpu_100ns != 0) {
            ++summary.active_samples;
        }
        summary.sample_cpu_ms.push_back(
            static_cast<double>(cpu_100ns) / 10000.0);
    }

    std::map<uint32_t, const ProcessThreadSnapshot*> thread_config_by_id{};
    for (const auto& thread : process_threads) {
        thread_config_by_id[thread.thread_id] = &thread;
    }
    std::map<uint32_t, std::set<uint32_t>> thread_roles{};
    for (const auto& sample : thread_snapshots) {
        if (sample.point < kThreadPointCount) {
            thread_roles[sample.thread_id].insert(sample.point);
        }
    }

    fprintf(file, "\nTHREAD_CPU_SUMMARY,tid,roles,samples,active_samples,"
        "observed_ms,kernel_ms,user_ms,total_cpu_ms,one_core_pct,cycles,"
        "p95_sample_cpu_ms,max_sample_cpu_ms,current_priority,name\n");
    for (const auto& [thread_id, summary] : thread_cpu_summaries) {
        std::ostringstream roles{};
        const auto role_found = thread_roles.find(thread_id);
        if (role_found != thread_roles.end()) {
            bool first = true;
            for (const uint32_t point : role_found->second) {
                if (!first) {
                    roles << ';';
                }
                roles << thread_point_name(point);
                first = false;
            }
        }
        const double observed_ms = frequency > 0
            ? static_cast<double>(summary.observed_qpc) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        const double kernel_ms =
            static_cast<double>(summary.kernel_100ns) / 10000.0;
        const double user_ms =
            static_cast<double>(summary.user_100ns) / 10000.0;
        const double total_ms = kernel_ms + user_ms;
        const double one_core_pct = observed_ms > 0.0
            ? total_ms * 100.0 / observed_ms
            : 0.0;
        const auto config_found = thread_config_by_id.find(thread_id);
        const ProcessThreadSnapshot* config = config_found !=
                thread_config_by_id.end()
            ? config_found->second
            : nullptr;
        const double maximum = summary.sample_cpu_ms.empty()
            ? 0.0
            : *std::max_element(summary.sample_cpu_ms.begin(),
                summary.sample_cpu_ms.end());
        fprintf(file,
            "THREAD_CPU_SUMMARY,%u,%s,%zu,%zu,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%llu,%.3f,%.3f,%d,%s\n",
            thread_id, roles.str().c_str(), summary.samples,
            summary.active_samples, observed_ms, kernel_ms, user_ms,
            total_ms, one_core_pct,
            static_cast<unsigned long long>(summary.cycles),
            percentile(summary.sample_cpu_ms, 0.95), maximum,
            config != nullptr ? config->relative_priority
                              : kUnknownThreadPriority,
            config != nullptr ? config->description.c_str() : "");
    }

    uint64_t process_kernel_100ns{};
    uint64_t process_user_100ns{};
    uint64_t process_observed_qpc{};
    uint64_t sampler_wall_ticks{};
    uint64_t failed_thread_queries{};
    std::vector<double> process_sample_cpu_ms{};
    std::vector<double> sampler_sample_wall_ms{};
    for (const auto& interval : process_cpu_intervals) {
        process_kernel_100ns += interval.kernel_100ns;
        process_user_100ns += interval.user_100ns;
        failed_thread_queries += interval.failed_threads;
        sampler_wall_ticks += interval.sampler_wall_ticks;
        if (interval.end_qpc >= interval.begin_qpc) {
            process_observed_qpc += static_cast<uint64_t>(
                interval.end_qpc - interval.begin_qpc);
        }
        process_sample_cpu_ms.push_back(static_cast<double>(
            interval.kernel_100ns + interval.user_100ns) / 10000.0);
        sampler_sample_wall_ms.push_back(frequency > 0
            ? static_cast<double>(interval.sampler_wall_ticks) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0);
    }
    const double process_observed_ms = frequency > 0
        ? static_cast<double>(process_observed_qpc) * 1000.0 /
            static_cast<double>(frequency)
        : 0.0;
    const double process_kernel_ms =
        static_cast<double>(process_kernel_100ns) / 10000.0;
    const double process_user_ms =
        static_cast<double>(process_user_100ns) / 10000.0;
    const double process_total_ms = process_kernel_ms + process_user_ms;
    const double equivalent_cores = process_observed_ms > 0.0
        ? process_total_ms / process_observed_ms
        : 0.0;
    const DWORD logical_processors =
        GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    const double machine_pct = logical_processors != 0
        ? equivalent_cores * 100.0 / static_cast<double>(logical_processors)
        : 0.0;
    const double sampler_total_wall_ms = frequency > 0
        ? static_cast<double>(sampler_wall_ticks) * 1000.0 /
            static_cast<double>(frequency)
        : 0.0;
    fprintf(file, "\nPROCESS_CPU_SUMMARY,samples,observed_ms,kernel_ms,"
        "user_ms,total_cpu_ms,average_equivalent_cores,machine_pct,"
        "logical_processors,p95_sample_cpu_ms,max_sample_cpu_ms,"
        "sampler_total_wall_ms,sampler_avg_wall_ms,sampler_max_wall_ms,"
        "failed_thread_queries\n");
    fprintf(file,
        "PROCESS_CPU_SUMMARY,%zu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%lu,"
        "%.3f,%.3f,%.3f,%.3f,%.3f,%llu\n",
        process_cpu_intervals.size(), process_observed_ms, process_kernel_ms,
        process_user_ms, process_total_ms, equivalent_cores, machine_pct,
        static_cast<unsigned long>(logical_processors),
        percentile(process_sample_cpu_ms, 0.95),
        process_sample_cpu_ms.empty() ? 0.0 : *std::max_element(
            process_sample_cpu_ms.begin(), process_sample_cpu_ms.end()),
        sampler_total_wall_ms,
        process_cpu_intervals.empty() ? 0.0 : sampler_total_wall_ms /
            static_cast<double>(process_cpu_intervals.size()),
        sampler_sample_wall_ms.empty() ? 0.0 : *std::max_element(
            sampler_sample_wall_ms.begin(), sampler_sample_wall_ms.end()),
        static_cast<unsigned long long>(failed_thread_queries));

    fprintf(file, "\nCPU_TOPOLOGY,group,logical_cpu,core_index,"
        "efficiency_class,scheduling_class,cpu_set_id,flags\n");
    for (const auto& topology : g_cpu_topology) {
        fprintf(file, "CPU_TOPOLOGY,%u,%u,%d,%d,%d,%u,%u\n",
            topology.group, topology.logical_processor, topology.core_index,
            topology.efficiency_class, topology.scheduling_class,
            topology.cpu_set_id, topology.flags);
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

    fprintf(file, "\nTHREAD,event,age_ms,frame,pair,point,eye,tid,group,"
        "logical_cpu,core_index,efficiency_class,scheduling_class,"
        "thread_priority\n");
    for (const auto& sample : thread_snapshots) {
        const double age_ms = frequency > 0
            ? static_cast<double>(sample.qpc - now) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        fprintf(file,
            "THREAD,%llu,%.3f,%llu,%llu,%s,%d,%u,%u,%u,%d,%d,%d,%d\n",
            static_cast<unsigned long long>(sample.sequence), age_ms,
            static_cast<unsigned long long>(sample.frame),
            static_cast<unsigned long long>(sample.pair_id),
            thread_point_name(sample.point), sample.eye, sample.thread_id,
            sample.group, sample.logical_processor, sample.core_index,
            sample.efficiency_class, sample.scheduling_class,
            sample.thread_priority);
    }

    fprintf(file, "\nPROCESS_CPU_SAMPLE,end_age_ms,elapsed_ms,kernel_ms,"
        "user_ms,total_cpu_ms,equivalent_cores,machine_pct,queried_threads,"
        "failed_threads,sampler_wall_ms\n");
    for (const auto& interval : process_cpu_intervals) {
        const double end_age_ms = frequency > 0
            ? static_cast<double>(interval.end_qpc - now) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        const double elapsed_ms = frequency > 0
            ? static_cast<double>(interval.end_qpc - interval.begin_qpc) *
                1000.0 / static_cast<double>(frequency)
            : 0.0;
        const double kernel_ms =
            static_cast<double>(interval.kernel_100ns) / 10000.0;
        const double user_ms =
            static_cast<double>(interval.user_100ns) / 10000.0;
        const double total_ms = kernel_ms + user_ms;
        const double sample_equivalent_cores = elapsed_ms > 0.0
            ? total_ms / elapsed_ms
            : 0.0;
        const double sample_machine_pct = logical_processors != 0
            ? sample_equivalent_cores * 100.0 /
                static_cast<double>(logical_processors)
            : 0.0;
        const double sample_sampler_wall_ms = frequency > 0
            ? static_cast<double>(interval.sampler_wall_ticks) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        fprintf(file,
            "PROCESS_CPU_SAMPLE,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%u,%u,%.3f\n",
            end_age_ms, elapsed_ms, kernel_ms, user_ms, total_ms,
            sample_equivalent_cores, sample_machine_pct,
            interval.queried_threads, interval.failed_threads,
            sample_sampler_wall_ms);
    }

    fprintf(file, "\nTHREAD_CPU_SAMPLE,end_age_ms,elapsed_ms,tid,kernel_ms,"
        "user_ms,total_cpu_ms,one_core_pct,cycles\n");
    for (const auto& interval : thread_cpu_intervals) {
        const double end_age_ms = frequency > 0
            ? static_cast<double>(interval.end_qpc - now) * 1000.0 /
                static_cast<double>(frequency)
            : 0.0;
        const double elapsed_ms = frequency > 0
            ? static_cast<double>(interval.end_qpc - interval.begin_qpc) *
                1000.0 / static_cast<double>(frequency)
            : 0.0;
        const double kernel_ms =
            static_cast<double>(interval.kernel_100ns) / 10000.0;
        const double user_ms =
            static_cast<double>(interval.user_100ns) / 10000.0;
        const double total_ms = kernel_ms + user_ms;
        const double one_core_pct = elapsed_ms > 0.0
            ? total_ms * 100.0 / elapsed_ms
            : 0.0;
        fprintf(file,
            "THREAD_CPU_SAMPLE,%.3f,%.3f,%u,%.3f,%.3f,%.3f,%.3f,%llu\n",
            end_age_ms, elapsed_ms, interval.thread_id, kernel_ms, user_ms,
            total_ms, one_core_pct,
            static_cast<unsigned long long>(interval.cycles));
    }
    fclose(file);
    return true;
}

} // namespace w3vr::pipeline_flight
