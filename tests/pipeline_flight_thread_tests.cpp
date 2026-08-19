#include "pipeline_flight_recorder.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

bool contains(const std::string& text, const char* expected) {
    if (text.find(expected) != std::string::npos) {
        return true;
    }
    std::cerr << "missing flight-recorder text: " << expected << '\n';
    return false;
}

std::set<std::filesystem::path> flight_logs(
    const std::filesystem::path& directory) {
    std::set<std::filesystem::path> paths{};
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const std::wstring name = entry.path().filename().wstring();
        if (entry.is_regular_file() &&
            name.starts_with(L"witcher3vr_pipeline_flight_") &&
            entry.path().extension() == L".log") {
            paths.insert(entry.path());
        }
    }
    return paths;
}

} // namespace

int main() {
    using namespace w3vr::pipeline_flight;

    wchar_t executable[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        std::cerr << "cannot resolve test executable path\n";
        return 1;
    }
    const auto output_directory =
        std::filesystem::path(executable).parent_path();
    const auto logs_before = flight_logs(output_directory);

    set_enabled(true);
    begin_frame(42, 3, 2, true, false, true, true);
    record_thread_sample(ThreadPoint::ProducerExit, 100, -1);
    record_thread_sample(ThreadPoint::RenderWorkerEntry, 100, 0);
    record_thread_sample(ThreadPoint::RenderWorkerEntry, 100, 1);
    const int64_t engine_begin = cpu_begin();
    Sleep(2);
    cpu_end(Phase::EngineGameplayOriginal, engine_begin);
    Sleep(600);
    if (!dump_last_ten_seconds()) {
        std::cerr << "flight-recorder dump failed\n";
        set_enabled(false);
        return 1;
    }
    if (!dump_last_ten_seconds()) {
        std::cerr << "second flight-recorder dump failed\n";
        set_enabled(false);
        return 1;
    }
    set_enabled(false);

    const auto logs_after = flight_logs(output_directory);
    std::vector<std::filesystem::path> new_logs{};
    for (const auto& path : logs_after) {
        if (!logs_before.contains(path)) {
            new_logs.push_back(path);
        }
    }
    if (new_logs.size() != 2 || new_logs[0] == new_logs[1]) {
        std::cerr << "expected two distinct timestamped flight logs, got "
                  << new_logs.size() << '\n';
        return 1;
    }

    const auto log_path = new_logs.front();
    std::ifstream input(log_path, std::ios::binary);
    const std::string log{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    input.close();
    for (const auto& path : new_logs) {
        DeleteFileW(path.c_str());
    }

    bool ok = true;
    ok &= contains(log, "WITCHER3VR_PIPELINE_FLIGHT_V3");
    ok &= contains(log, "build=V1259 base=V1258");
    ok &= contains(log, "file_policy=timestamped_create_new_no_overwrite");
    ok &= contains(log, "flags: bit0=asymmetric bit1=rtx bit2=afw "
        "bit3=afw_visual_debug");
    ok &= contains(log, ",3,2,13,");
    ok &= contains(log, "THREAD_ROLE_SUMMARY,producer_exit,1,1,");
    ok &= contains(log, "THREAD_ROLE_SUMMARY,render_worker_entry,2,1,");
    ok &= contains(log, "THREAD_PLACEMENT_SUMMARY,producer_exit,");
    ok &= contains(log, "PROCESS_THREAD_CONFIG,tid,base_priority");
    ok &= contains(log, "THREAD_CPU_SUMMARY,tid,roles,samples");
    ok &= contains(log, "PROCESS_CPU_SUMMARY,samples,observed_ms");
    ok &= contains(log, "PROCESS_CPU_SAMPLE,end_age_ms,elapsed_ms");
    ok &= contains(log, "THREAD_CPU_SAMPLE,end_age_ms,elapsed_ms");
    ok &= contains(log, "SUMMARY,cpu,engine_gameplay_original");
    ok &= contains(log, "CPU_TOPOLOGY,group,logical_cpu,core_index");
    ok &= contains(log, ",100,producer_exit,-1,");
    ok &= contains(log, ",100,render_worker_entry,0,");
    ok &= contains(log, ",100,render_worker_entry,1,");
    return ok ? 0 : 1;
}
