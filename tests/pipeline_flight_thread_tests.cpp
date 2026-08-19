#include "pipeline_flight_recorder.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool contains(const std::string& text, const char* expected) {
    if (text.find(expected) != std::string::npos) {
        return true;
    }
    std::cerr << "missing flight-recorder text: " << expected << '\n';
    return false;
}

} // namespace

int main() {
    using namespace w3vr::pipeline_flight;

    set_enabled(true);
    begin_frame(42, 3, 2, true, false, true, true);
    record_thread_sample(ThreadPoint::ProducerExit, 100, -1);
    record_thread_sample(ThreadPoint::RenderWorkerEntry, 100, 0);
    record_thread_sample(ThreadPoint::RenderWorkerEntry, 100, 1);
    if (!dump_last_ten_seconds()) {
        std::cerr << "flight-recorder dump failed\n";
        return 1;
    }

    wchar_t executable[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        std::cerr << "cannot resolve test executable path\n";
        return 1;
    }
    const auto log_path = std::filesystem::path(executable).parent_path() /
        L"witcher3vr_pipeline_flight.log";
    std::ifstream input(log_path, std::ios::binary);
    const std::string log{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    input.close();
    DeleteFileW(log_path.c_str());

    bool ok = true;
    ok &= contains(log, "WITCHER3VR_PIPELINE_FLIGHT_V2");
    ok &= contains(log, "build=V1242");
    ok &= contains(log, "flags: bit0=asymmetric bit1=rtx bit2=afw "
        "bit3=afw_visual_debug");
    ok &= contains(log, ",3,2,13,");
    ok &= contains(log, "THREAD_ROLE_SUMMARY,producer_exit,1,1,");
    ok &= contains(log, "THREAD_ROLE_SUMMARY,render_worker_entry,2,1,");
    ok &= contains(log, "THREAD_PLACEMENT_SUMMARY,producer_exit,");
    ok &= contains(log, "PROCESS_THREAD_CONFIG,tid,base_priority");
    ok &= contains(log, "CPU_TOPOLOGY,group,logical_cpu,core_index");
    ok &= contains(log, ",100,producer_exit,-1,");
    ok &= contains(log, ",100,render_worker_entry,0,");
    ok &= contains(log, ",100,render_worker_entry,1,");
    return ok ? 0 : 1;
}
