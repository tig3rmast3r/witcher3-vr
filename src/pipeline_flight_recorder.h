#pragma once

#include <d3d12.h>

#include <cstdint>

namespace w3vr::pipeline_flight {

// CPU phases are recorded on every frame. GPU phases are sampled at a fixed
// cadence and resolved asynchronously after their real submission queue fence.
enum class Phase : uint32_t {
    FrameFactory,
    RtxAo,
    RtxShadow,
    RtxReflection,
    RtxStabilize,
    TemporalDlss,
    TemporalTaau,
    Afw,
    XrWaitFrame,
    XrWaitSwapchain,
    XrAllocatorWait,
    XrRecordComposite,
    XrEndFrame,
    XrFrame,
    QueueSubmit,
    Present,
    Count
};

// Exact inline observation points in REDengine's stereo path. These describe
// where the instrumented code was scheduled at that instant; they do not infer
// placement for the rest of the engine worker pool.
enum class ThreadPoint : uint32_t {
    ProducerExit,
    RenderWorkerEntry,
    Count
};

struct GpuToken {
    uint32_t slot{UINT32_MAX};
    uint32_t generation{};
};

void set_enabled(bool enabled);
bool enabled();

void begin_frame(
    uint64_t frame,
    int openxr_mode,
    int temporal_backend,
    bool native_asymmetric,
    bool raytracing,
    bool afw,
    bool afw_visual_debug);

// Lock-free hot-path sample. CPU topology is cached when the recorder is
// enabled, and thread priority is refreshed at a low fixed cadence.
void record_thread_sample(
    ThreadPoint point,
    uint64_t pair_id = 0,
    int eye = -1);

int64_t cpu_begin();
void cpu_end(Phase phase, int64_t begin, uint64_t frame = UINT64_MAX);

class CpuScope {
public:
    explicit CpuScope(Phase phase, uint64_t frame = UINT64_MAX);
    ~CpuScope();

    CpuScope(const CpuScope&) = delete;
    CpuScope& operator=(const CpuScope&) = delete;

private:
    Phase phase_;
    uint64_t frame_;
    int64_t begin_;
};

GpuToken gpu_begin(Phase phase, ID3D12GraphicsCommandList* command_list);
void gpu_end(GpuToken token, ID3D12GraphicsCommandList* command_list);

class GpuScope {
public:
    GpuScope(Phase phase, ID3D12GraphicsCommandList* command_list);
    ~GpuScope();

    GpuScope(const GpuScope&) = delete;
    GpuScope& operator=(const GpuScope&) = delete;

private:
    ID3D12GraphicsCommandList* command_list_{};
    GpuToken token_{};
};

// Call immediately after the real ExecuteCommandLists. Samples are associated
// with the exact queue which submitted their command list.
void on_execute(
    ID3D12CommandQueue* queue,
    UINT num_command_lists,
    ID3D12CommandList* const* command_lists);

// Nonblocking: only consumes query results whose submission fence has retired.
void process_gpu();

// Overwrites witcher3vr_pipeline_flight.log beside this DLL with the most
// recent ten seconds. File I/O happens only on this explicit F2 action.
bool dump_last_ten_seconds();

} // namespace w3vr::pipeline_flight
