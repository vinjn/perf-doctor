#define _HAS_STD_BYTE 0

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/ConcurrentCircularBuffer.h"
#include "cinder/Json.h"

#include "MixDevice.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"

using namespace ci;
using namespace ci::app;
using namespace std;

float global_min_t = 0;
float global_max_t = 1;
uint64_t firstFrameTimestamp = 0;
uint64_t deltaTimestamp = 0;
uint64_t firstCpuStatTimestamp = 0;
int pid;

struct Span
{
    string name;
    string label;
    float start = 0;
    float end = 0;
};

struct MemoryStat
{
    float pssTotal;
    float pssGL;
    float pssEGL;
    float pssGfx;
    float pssUnknown;
    float pssNativeHeap;

    float privateClean;
    float privateDirty;
};

struct TemperatureStatSlot
{
    int cpu = -1;
    int gpu = -1;
    int battery = -1;
};

struct TemperatureStat
{
    float cpu = 0, gpu = 0, battery = 0;
};

struct CpuConfig
{
    int id;
    int cpuinfo_min_freq = 0, cpuinfo_max_freq = 0;
    string features;
    string implementer;
    string architecture;
    string variant;
    string part;
    string revision;
};

struct DeviceStat
{
    int width = 0, height = 0;
    string os_version;
    string gfx_api_version;
    string market_name;
    string hardware;
};

string getTimestampForFilename()
{
    char buffer[256];
    time_t rawtime;
    time(&rawtime);
    tm* timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y_%b_%d-%H_%M_%S ", timeinfo);

    return string(buffer);
}

string perfettoCmdTemplate = R"(
buffers: {
    size_kb: 63488
    fill_policy: DISCARD
}
buffers: {
    size_kb: 2048
    fill_policy: DISCARD
}
data_sources: {
    config {
        name: "linux.process_stats"
        target_buffer: 1
        process_stats_config {
            scan_all_processes_on_start: true
        }
    }
}
data_sources: {
    config {
        name: "linux.ftrace"
        ftrace_config {
            ftrace_events: "gfx"
            ftrace_events: "sched/sched_switch"
            ftrace_events: "power/suspend_resume"
            ftrace_events: "sched/sched_wakeup"
            ftrace_events: "sched/sched_wakeup_new"
            ftrace_events: "sched/sched_waking"
            ftrace_events: "power/cpu_frequency"
            ftrace_events: "power/cpu_idle"
            ftrace_events: "power/gpu_frequency"
            ftrace_events: "sched/sched_process_exit"
            ftrace_events: "sched/sched_process_free"
            ftrace_events: "task/task_newtask"
            ftrace_events: "task/task_rename"
            ftrace_events: "ftrace/print"
            atrace_categories: "gfx"
            atrace_categories: "power"
            atrace_apps: "ATRACE_APP_NAME"
        }
    }
}
data_sources: {
  config {
    name: "android.surfaceflinger.frame"
  }
}
data_sources: {
  config {
    name: "gpu.counters"
    target_buffer: 0
    legacy_config: "counter_period_ns: 1000000\ncounter_ids:3\ncounter_ids:4\ncounter_ids:260\ncounter_ids:261\ncounter_ids:262\ncounter_ids:7\ncounter_ids:263\ncounter_ids:264\ncounter_ids:265\ncounter_ids:266\ncounter_ids:267\ncounter_ids:268\ncounter_ids:269\ncounter_ids:270\ncounter_ids:15\ncounter_ids:271\ncounter_ids:276\ncounter_ids:277\ncounter_ids:278\ncounter_ids:30\ncounter_ids:32\ncounter_ids:291\ncounter_ids:292\ncounter_ids:37\ncounter_ids:293\ncounter_ids:294\ncounter_ids:297\ncounter_ids:298\ncounter_ids:299\ncounter_ids:300\ncounter_ids:301\ncounter_ids:302\ncounter_ids:308\ncounter_ids:309\ncounter_ids:311\ncounter_ids:312\ncounter_ids:313\ncounter_ids:314\ncounter_ids:315\ncounter_ids:86\ncounter_ids:87\ncounter_ids:88\ncounter_ids:93\ncounter_ids:94\ncounter_ids:95\ncounter_ids:351\ncounter_ids:96\ncounter_ids:352\ncounter_ids:97\ncounter_ids:353\ncounter_ids:98\ncounter_ids:354\ncounter_ids:355\ncounter_ids:356\ncounter_ids:104\ncounter_ids:108\ncounter_ids:110\ncounter_ids:111\ncounter_ids:117\ncounter_ids:125\nset_id:0\n"
  }
}
data_sources: {
  config {
    name: "gpu.renderstages"
  }
}
data_sources: {
  config {
    name: "VulkanAPI"
  }
}
data_sources: {
  config {
    name: "VulkanCPUTiming"
    legacy_config: "VkDevice:VkPhysicalDevice:VkInstance:VkQueue"
  }
}

duration_ms: 1000
)";

struct CpuStat
{
    int cpu_id;
    long int user, nice, sys, idle, iowait, irq, softirq;
    int freq;

    CpuStat(const string& line)
    {
        char cpu[5]; // TODO: remove
        sscanf(line.c_str(), "%s%d%d%d%d%d%d%d", cpu, &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
        cpu_id = cpu[3] - '0';
    }

    long int getAll() const
    {
        return user + nice + sys + idle + iowait + irq + softirq;
    }

    long int getIdle() const
    {
        return idle;
    }
};

struct AppCpuStat
{
    // https://www.chenwenguan.com/android-performance-monitor-cpu/
    long int utime, stime;
    long int cutime, cstime;

    AppCpuStat(const string& line)
    {
        auto tokens = split(line, ' ');
        utime = fromString<long int>(tokens[14]);
        stime = fromString<long int>(tokens[15]);
        cutime = fromString<long int>(tokens[16]);
        cstime = fromString<long int>(tokens[17]);
    }

    long int getActiveTime() const
    {
        return utime + stime + cutime + cstime;
    }
};

static float calcCpuUsage(const CpuStat& lhs, const CpuStat& rhs)
{
    auto totalTime = rhs.getAll() - lhs.getAll();
    auto idleTime = rhs.getIdle() - lhs.getIdle();
    auto usage = (totalTime - idleTime) * 100.0f / totalTime;
    return usage;
}

// TODO: so ugly
vector<pair<uint64_t, CpuStat>> mCpuStats;
vector<CpuConfig> mCpuConfigs;

static float calcAppCpuUsage(const CpuStat& lhs, const CpuStat& rhs, const AppCpuStat& appLhs, const AppCpuStat& appRhs)
{
    auto totalTime = rhs.getAll() - lhs.getAll();
    auto appActiveTime = appRhs.getActiveTime() - appLhs.getActiveTime();
    auto usage = appActiveTime * mCpuConfigs.size() * 100.0f / totalTime;
    return usage;
}

struct SpanSeries
{
    string name;
    vector<Span> span_array;

    SpanSeries()
    {
    }

    // Plots axis-aligned, filled rectangles. Every two consecutive points defines opposite corners of a single rectangle.
    static ImPlotPoint getter(void* data, int idx)
    {
        auto* self = (SpanSeries*)data;
        int span_idx = idx / 2;
        int tag = idx % 2;
        if (tag == 0)
        {
            float start_t = self->span_array[span_idx].start/*  / TIME_UNIT_SCALE */;
            return ImPlotPoint(start_t, 0);
        }
        else
        {
            float end_t = self->span_array[span_idx].end/*  / TIME_UNIT_SCALE */;
            return ImPlotPoint(end_t, 10);
        }
    }
};

struct MetricSeries
{
    bool visible = true;
    MetricSeries()
    {

    }

    static ImPlotPoint getter(void* data, int idx)
    {
        auto* self = (MetricSeries*)data;
        return ImPlotPoint(self->t_array[idx], self->x_array[idx]);
    }

    float min_x = 0;
    float max_x = 60;

    string name;
    vector<float> t_array;
    vector<float> x_array;
};

struct DataStorage
{
    unordered_map<string, SpanSeries> span_storage;
    unordered_map<string, MetricSeries> metric_storage;
};

#include <windows.h>
#include <Dbghelp.h>
#include <shlobj_core.h>
#include <winerror.h>

void goto_folder(fs::path folder, fs::path selectPath)
{
    PIDLIST_ABSOLUTE pidl;
    if (SUCCEEDED(SHParseDisplayName(folder.wstring().c_str(), 0, &pidl, 0, 0)))
    {
        // we don't want to actually select anything in the folder, so we pass an empty
        // PIDL in the array. if you want to select one or more items in the opened
        // folder you'd need to build the PIDL array appropriately

        if (selectPath.empty())
        {
            ITEMIDLIST idNull = { 0 };
            LPCITEMIDLIST pidlNull[1] = { &idNull };
            SHOpenFolderAndSelectItems(pidl, 1, pidlNull, 0);
        }
        else
        {
            ITEMIDLIST* pidlist;
            SFGAOF aog;
            SHParseDisplayName(selectPath.wstring().c_str(), NULL, &pidlist, SFGAO_CANCOPY, &aog);//pidlist就是转换以后的shell路径

            LPCITEMIDLIST pidlNull[1] = { pidlist };
            SHOpenFolderAndSelectItems(pidl, 1, pidlNull, 0);
            ILFree(pidlist);
        }

        ILFree(pidl);
    }
}


void make_minidump(EXCEPTION_POINTERS* e)
{
    auto hDbgHelp = LoadLibraryA("dbghelp");
    if (hDbgHelp == nullptr)
        return;
    auto pMiniDumpWriteDump = (decltype(&MiniDumpWriteDump))GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (pMiniDumpWriteDump == nullptr)
        return;

    char name[MAX_PATH];
    {
        auto nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);
        SYSTEMTIME t;
        GetSystemTime(&t);
        wsprintfA(nameEnd - strlen(".exe"),
            "_%4d%02d%02d_%02d%02d%02d.dmp",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    }

    auto hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = e;
    exceptionInfo.ClientPointers = FALSE;

    auto dumped = pMiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
        e ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);

    CloseHandle(hFile);

    return;
}

LONG CALLBACK unhandled_handler(EXCEPTION_POINTERS* e)
{
    make_minidump(e);
    return EXCEPTION_CONTINUE_SEARCH;
}

int runCmd(const string& cmd, std::string& outOutput)
{
    CI_LOG_W(cmd);

    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    HANDLE g_hChildStd_ERR_Rd = NULL;
    HANDLE g_hChildStd_ERR_Wr = NULL;

    SECURITY_ATTRIBUTES sa;
    // Set the bInheritHandle flag so pipe handles are inherited.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &sa, 0)) { return 1; } // Create a pipe for the child process's STDERR.
    if (!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) { return 1; } // Ensure the read handle to the pipe for STDERR is not inherited.
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)) { return 1; } // Create a pipe for the child process's STDOUT.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) { return 1; } // Ensure the read handle to the pipe for STDOUT is not inherited

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    bool bSuccess = FALSE;

    // Set up members of the PROCESS_INFORMATION structure.
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure.
    // This structure specifies the STDERR and STDOUT handles for redirection.
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = g_hChildStd_ERR_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process.
    bSuccess = CreateProcessA(
        NULL,             // program name
        (char*)cmd.c_str(),       // command line
        NULL,             // process security attributes
        NULL,             // primary thread security attributes
        TRUE,             // handles are inherited
        CREATE_NO_WINDOW, // creation flags (this is what hides the window)
        NULL,             // use parent's environment
        NULL,             // use parent's current directory
        &siStartInfo,     // STARTUPINFO pointer
        &piProcInfo       // receives PROCESS_INFORMATION
    );

    CloseHandle(g_hChildStd_ERR_Wr);
    CloseHandle(g_hChildStd_OUT_Wr);

    // read output
#define BUFSIZE 4096
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    bool bSuccess2 = FALSE;
    for (;;) { // read stdout
        bSuccess2 = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess2 || dwRead == 0) break;
        std::string s(chBuf, dwRead);
        outOutput += s;
    }
    dwRead = 0;
    for (;;) { // read stderr
        bSuccess2 = ReadFile(g_hChildStd_ERR_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess2 || dwRead == 0) break;
        std::string s(chBuf, dwRead);
        outOutput += s;
    }

    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_ERR_Rd);

    CI_LOG_W(outOutput);

    // The remaining open handles are cleaned up when this process terminates.
    // To avoid resource leaks in a larger application,
    // close handles explicitly.
    return 0;
}

static ImPlotPoint frameTime_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, uint64_t>>*)data;
    return ImPlotPoint((self[idx].first - firstFrameTimestamp) * 1e-3, self[idx].second);
}

static ImPlotPoint fps_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, float>>*)data;
    return ImPlotPoint((self[idx].first - firstFrameTimestamp) * 1e-3, self[idx].second);
}

static ImPlotPoint cpuUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, CpuStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, calcCpuUsage(self[idx].second, self[idx + 1].second));
}

static ImPlotPoint cpuFreq_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, CpuStat>>*)data;
    int cpu_id = self[idx].second.cpu_id;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.freq * 100 / mCpuConfigs[cpu_id].cpuinfo_max_freq);
}

static ImPlotPoint app_cpuUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, AppCpuStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, calcAppCpuUsage(
        mCpuStats[idx].second, mCpuStats[idx + 1].second,
        self[idx].second, self[idx + 1].second));
}

static ImPlotPoint memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.pssTotal);
}

static ImPlotPoint gl_memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.pssGL + self[idx].second.pssEGL + self[idx].second.pssGfx);
}

static ImPlotPoint nativeheap_memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.pssNativeHeap);
}

static ImPlotPoint unknown_memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.pssUnknown);
}

static ImPlotPoint privateClean_memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.privateClean);
}

static ImPlotPoint privateDirty_memoryUsage_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, MemoryStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.privateDirty);
}

static ImPlotPoint temp_cpu_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, TemperatureStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.cpu);
}

static ImPlotPoint temp_gpu_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, TemperatureStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.gpu);
}

static ImPlotPoint temp_battery_getter(void* data, int idx)
{
    const auto& self = *(vector<pair<uint64_t, TemperatureStat>>*)data;
    return ImPlotPoint((self[idx].first - firstCpuStatTimestamp) * 1e-3, self[idx].second.battery);
}

struct AdbResults
{
    bool success = true;
    vector<string> SurfaceFlinger_latency;
    vector<string> EPOCHREALTIME;
    vector<string> proc_stat;
    vector<string> proc_pid_stat;
    vector<string> dump_sys_meminfo;
    vector<string> proc_pid_smaps_rollup;
    vector<string> scaling_cur_freq;
    TemperatureStat temperature;
};

struct PerfDoctorApp : public App
{
    DataStorage storage;
    ImPlotContext* implotCtx = nullptr;

    ConcurrentCircularBuffer<AdbResults> mAdbResults{ 2 };
    ConcurrentCircularBuffer<string> mAsyncCommands{ 2 };
    unique_ptr<thread> mAdbThread;
    bool mIsRunning = true;
    bool mAutoStart = false;

    MixDevice mMixDevice;

    FILE* fp_idb;
    vector<string> executeIdb(string cmd, bool async = false, bool oneDeviceOnly = true)
    {
        char fullCmd[256];
        if (oneDeviceOnly)
            sprintf(fullCmd, "tidevice -u %s %s", mSerialNames[DEVICE_ID].c_str(), cmd.c_str());
        else
            sprintf(fullCmd, "tidevice %s", cmd.c_str());

#if 1
        fp_idb = nullptr;
        fp_idb = _popen(fullCmd, "r");  // Open the command for reading.
        if (fp_idb == NULL) {
            return {};
        }
#else
        string result;
        runCmd(fullCmd, result);
        if (result.empty()) return {};
        auto lines = split(result, "\r\n");
        if (lines[lines.size() - 1].empty())
            lines.pop_back();
        return lines;
#endif
    }

    vector<string> executeAdb(string cmd, bool oneDeviceOnly = true)
    {
        static bool init = true;
        static string adbExe = "adb";
        if (init)
        {
            init = false;
            string result;
            runCmd("where adb", result);
            if (result.find("adb") == string::npos)
                adbExe = (getAppPath() / "adb" / "adb.exe").string();
        }
        char fullCmd[256];
        if (oneDeviceOnly)
            sprintf(fullCmd, "%s -s %s %s", adbExe.c_str(), mSerialNames[DEVICE_ID].c_str(), cmd.c_str());
        else
            sprintf(fullCmd, "%s %s", adbExe.c_str(), cmd.c_str());

        string result;
        runCmd(fullCmd, result);
        if (result.empty()) return {};
        auto lines = split(result, "\r\n");
        if (lines[lines.size() - 1].empty())
            lines.pop_back();
        return lines;
    }

    int mAppId = -1;
    vector<string> mSerialNames;
    vector<string> mDeviceNames;
    vector<bool> mIsIOSDevices;
    vector<string> mAppNames;
    string mSurfaceViewName = "";
    string mPackageName = "";
    bool mIsProfiling = false;
    float mLastUpdateTime = 0;
    vector<pair<uint64_t, uint64_t>> mFrameTimes;
    vector<uint64_t> mTimestamps; //ms
    vector<pair<uint64_t, float>> mFpsArray;
    vector<pair<uint64_t, AppCpuStat>> mAppCpuStats;
    vector<pair<uint64_t, CpuStat>> mChildCpuStats[8];
    vector<pair<uint64_t, MemoryStat>> mMemoryStats;

    TemperatureStatSlot mTemparatureStatSlot;
    vector<pair<uint64_t, TemperatureStat>> mTemperatureStats;

    vector<string> mUnrealCmds;
    
    uint64_t mLastSnapshotTs = 0;
    uint64_t mLastSnapshotIdx = 0;
    DeviceStat mDeviceStat;

    struct MetricSummary
    {
        float Min, Max, Avg;
        void reset()
        {
            Min = 0;
            Max = 0;
            Avg = 0;
        }

        void update(float new_value, int count)
        {
            if (new_value > Max) Max = new_value;
            if (new_value < Min) Min = new_value;
            Avg = (Avg * count + new_value) / (count + 1);
        }
    };

    MetricSummary mFpsSummary, mMemorySummary, mAppCpuSummary, mCpuTempSummary;
    int mDeviceId = -1;

    bool refreshDeviceNames()
    {
        mSerialNames.clear();
        mDeviceNames.clear();
        mTemparatureStatSlot = { -1, -1, -1 };

        char cmd[256];

        auto adbDeviceResults = executeAdb("devices", false);

        for (int i = 1; i < adbDeviceResults.size(); i++)
        {
            auto tokens = split(adbDeviceResults[i], '\t');
            if (tokens.size() < 2) continue;

            if (tokens[1] != "device")
            {
                dispatchAsync([] {
                    ImGui::OpenPopup("Device is unauthorized");

                    // Always center this window when appearing
                    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                    if (ImGui::BeginPopupModal("Device is unauthorized", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        ImGui::Text("Please authorize it first.\n");
                        ImGui::Separator();

                        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                        ImGui::SetItemDefaultFocus();
                        ImGui::EndPopup();
                    }
                });
                continue;
            }

            auto serial = tokens[0];

            mSerialNames.push_back(serial);

            sprintf(cmd, "-s %s shell getprop ro.product.model", serial.c_str());
            auto modelResults = executeAdb(cmd, false);

            if (serial.find(":5555") != string::npos)
                mDeviceNames.push_back(modelResults[0] + " [wifi]");
            else
                mDeviceNames.push_back(modelResults[0]);
            mIsIOSDevices.push_back(false);
        }

        int serialCount = mSerialNames.size();
        for (int i = 0; i < serialCount; i++)
        {
            if (mSerialNames[i].find(":5555") != string::npos)
            {
                continue;
            }

            sprintf(cmd, "-s %s shell \"ip addr show wlan0 | grep -e wlan0$ | cut -d\\\" \\\" -f 6 | cut -d/ -f 1\"", mSerialNames[i].c_str());
            auto ipResults = executeAdb(cmd, false);
            if (ipResults.empty()) continue;

            auto ipAddress = ipResults[0];
            int idx = 0;
            bool connected = false;
            for (const auto& name : mSerialNames)
            {
                if (name.find(ipAddress) != string::npos)
                {
                    connected = true;
                    break;
                }
                idx++;
            }
            if (!connected)
            {
                mSerialNames.push_back(ipAddress);
                mDeviceNames.push_back(mDeviceNames[i] + " [wifi]");
                mIsIOSDevices.push_back(false);
            }
        }

#if 0
        auto idbDeviceResults = executeIdb("list", false, false);
        for (int i = 0; i < idbDeviceResults.size(); i++)
        {
            auto tokens = split(idbDeviceResults[i], '\t');
            if (tokens.size() < 3) continue;

            mSerialNames.push_back(tokens[0]);
            mDeviceNames.push_back(tokens[1] + " [iOS]");
            mIsIOSDevices.push_back(true);
        }
#endif

        if (mSerialNames.empty())
            DEVICE_ID = -1;
        else
        {
            // TODO: ugly
            if (DEVICE_ID == -1 || DEVICE_ID >= mSerialNames.size())
                DEVICE_ID = 0;
        }

        return true;
    }

    bool refreshDeviceDetails_ios()
    {
        storage.metric_storage["frame_time"].visible = false;
        storage.metric_storage["fps"].visible = true;
        storage.metric_storage["cpu_usage"].visible = false;
        storage.metric_storage["core_usage"].visible = false;
        storage.metric_storage["memory_usage"].visible = false;
        storage.metric_storage["cpu_freq"].visible = false;
        storage.metric_storage["temperature"].visible = false;

        auto lines = executeIdb("-c applist");
        for (auto& line : lines)
        {
            auto tokens = split(line, '\t');
            const auto& appName = tokens[0];

            mAppNames.push_back(appName);
        }
        sort(mAppNames.begin(), mAppNames.end());

        if (!APP_NAME.empty())
        {
            int idx = 0;
            for (auto& name : mAppNames)
            {
                if (name == APP_NAME)
                {
                    mAppId = idx;
                    break;
                }
                idx++;
            }
        }

        lines = executeIdb("info");
        {
            for (auto& line : lines)
            {
                auto tokens = split(line, ":\t");
                auto key = tokens[0];
                auto value = tokens[1];
                if (key == "MarketName")
                    mDeviceStat.market_name = value;
                else if (key == "ProductVersion")
                    mDeviceStat.os_version = "iOS " + value;
            }
        }

        return true;
    }

    bool refreshDeviceDetails()
    {
        mAppNames.clear();
        mIsProfiling = false;
        mAppId = -1;

        mCpuConfigs.clear();

        if (DEVICE_ID == -1) return true;

        if (mIsIOSDevices[DEVICE_ID]) return refreshDeviceDetails_ios();

        storage.metric_storage["frame_time"].visible = false;
        storage.metric_storage["fps"].visible = true;
        storage.metric_storage["cpu_usage"].visible = true;
        storage.metric_storage["core_usage"].visible = true;
        storage.metric_storage["memory_usage"].visible = true;
        storage.metric_storage["cpu_freq"].visible = false;
        storage.metric_storage["temperature"].visible = true;

        if (count(mSerialNames[DEVICE_ID].begin(), mSerialNames[DEVICE_ID].end(), '.') == 3)
        {
            // if ip address
            char cmd[256];
            if (mSerialNames[DEVICE_ID].find(":5555") == string::npos)
            {
                // connect if not already connected
                sprintf(cmd, "-s %s tcpip 5555", mSerialNames[DEVICE_ID - 1].c_str());
                auto tcpipResults = executeAdb(cmd, false);

                sprintf(cmd, "connect %s", mSerialNames[DEVICE_ID].c_str());
                auto connectResults = executeAdb(cmd, false);
                mSerialNames[DEVICE_ID] += ":5555";
            }
        }
        auto lines = executeAdb("shell cat /proc/cpuinfo");
        for (auto& line : lines)
        {
            auto tokens = split(line, "\t:");
            if (tokens.size() != 2) continue;

            const auto& key = tokens[0];
            const auto& value = tokens[1];
            if (key == "processor")
            {
                mCpuConfigs.push_back({});
                mCpuConfigs.back().id = mCpuConfigs.size() - 1;
            }
            if (key == "Features")
            {
                mCpuConfigs.back().features = value;
            }
            if (key == "CPU implementer")
            {
                auto src = stoi(value, 0, 16);
                string name = value;
                switch (src)
                {
                case 'A': name = "arm"; break;
                case 'B': name = "broadcom"; break;
                case 'C': name = "cavium"; break;
                case 'H': name = "huawei"; break;
                case 'i': name = "intel"; break;
                case 'N': name = "nvidia"; break;
                case 'P': name = "apm"; break;
                case 'Q': name = "qualcomm"; break;
                case 'S': name = "samsung"; break;
                case 'V': name = "marvell"; break;
                }
                mCpuConfigs.back().implementer = name;
            }
            if (key == "CPU architecture") mCpuConfigs.back().architecture = value;
            if (key == "CPU variant") mCpuConfigs.back().variant = value;
            if (key == "CPU part")
            {
                auto src = stoi(value, 0, 16);
                string name = value;
                switch (src)
                {
                    // huawei
                case 0xD40: name = "Cortex-A76 Big/Medium cores"; break;
                    // arm
                case 0xC05: name = "Cortex-A5"; break;
                case 0xC07: name = "Cortex-A7"; break;
                case 0xC08: name = "Cortex-A8"; break;
                case 0xC09: name = "Cortex-A9"; break;
                case 0xC0C: name = "Cortex-A12"; break;
                case 0xC0E: name = "Cortex-A17"; break;
                case 0xC0D: name = "Cortex-A12"; break;
                case 0xC0F: name = "Cortex-A15"; break;
                case 0xD01: name = "Cortex-A32"; break;
                case 0xD03: name = "Cortex-A53"; break;
                case 0xD04: name = "Cortex-A35"; break;
                case 0xD05: name = "Cortex-A55"; break;
                case 0xD06: name = "Cortex-A65"; break;
                case 0xD07: name = "Cortex-A57"; break;
                case 0xD08: name = "Cortex-A72"; break;
                case 0xD09: name = "Cortex-A73"; break;
                case 0xD0A: name = "Cortex-A75"; break;
                case 0xD0B: name = "Cortex-A76"; break;
                case 0xD0C: name = "Neoverse-N1"; break;
                case 0xD0D: name = "Cortex-A77"; break;
                case 0xD0E: name = "Cortex-A76"; break;
                case 0xD41: name = "Cortex-A78"; break;
                case 0xD44: name = "Cortex-x1"; break;
                case 0xD4A: name = "Neoverse-E1"; break;
                    // qualcomm
                case 0x00F: name = "Cortex-A5"; break;
                case 0x02D: name = "Scorpion"; break;
                case 0x04D: name = "Dual-core Krait"; break;
                case 0x06F: name = "Quad-core Krait"; break;
                case 0x201: name = "Low-power Kryo"; break;
                case 0x205: name = "High-perf Kryo"; break;
                case 0x211: name = "Low-power Kryo"; break;
                case 0x800: name = "Cortex-A73 High-perf Kryo"; break;
                case 0x801: name = "Cortex-A53 Low-power Kryo"; break;
                case 0x802: name = "Cortex-A75 High-perf Kryo"; break;
                case 0x803: name = "Cortex-A55r0 Low-power Kryo"; break;
                case 0x804: name = "Cortex-A76 High-perf Kryo"; break;
                case 0x805: name = "Cortex-A55 Low-power Kryo"; break;
                case 0xC00: name = "Falkor"; break;
                case 0xC01: name = "Saphira"; break;
                }
                mCpuConfigs.back().part = name;
            }
            if (key == "CPU revision") mCpuConfigs.back().revision = value;
            if (key == "Hardware") mDeviceStat.hardware = value;
        }

        {
            auto lines = executeAdb("shell cat /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_min_freq");
            for (int i = 0; i < lines.size(); i++)
                mCpuConfigs[i].cpuinfo_min_freq = stoi(lines[i]);

            lines = executeAdb("shell cat /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq");
            for (int i = 0; i < lines.size(); i++)
                mCpuConfigs[i].cpuinfo_max_freq = stoi(lines[i]);
        }

        {
            auto lines = executeAdb("shell cat /sys/devices/virtual/thermal/thermal_zone*/type");
            for (int i = 0; i < lines.size(); i++)
            {
                if (mTemparatureStatSlot.cpu == -1 && (
                    lines[i].find("cpuss-") != string::npos
                    || lines[i].find("soc_thermal") != string::npos
                    ))
                    mTemparatureStatSlot.cpu = i;
                if (mTemparatureStatSlot.gpu == -1 && lines[i].find("gpuss-") != string::npos)
                    mTemparatureStatSlot.gpu = i;
     
                if (mTemparatureStatSlot.battery == -1 && (
                    lines[i].find("battery") != string::npos
                    || lines[i].find("Battery") != string::npos
                    ))
                    mTemparatureStatSlot.battery = i;
            }
        }

        if (LIST_ALL_APP)
            lines = executeAdb("shell pm list packages");
        else
            lines = executeAdb("shell pm list packages -3");
        for (auto& line : lines)
        {
            auto tokens = split(line, ':');
            const auto& appName = tokens[1];
#if 0
            bool appToIgnore = false;
            const string igoredApps[] = {
                "com.android.",
                "com.google.",

                "org.lineageos.",

                "com.qualcomm.",
                ".qti.",

                "com.samsung.",

                "com.vivo.",
                "com.bbk.",
                "com.iqoo.",

                "com.oppo.",
                "com.color.",
                "com.coloros.",
                "com.oplus.",
                "com.heytap.",

                "com.mi.",
                "miui.",
                "com.xiaomi.",

                "com.huawei.",
            };
            for (auto& keyword : igoredApps)
            {
                if (appName.find(keyword) != string::npos)
                {
                    appToIgnore = true;
                    break;
                }
            }
            if (appToIgnore) continue;
#endif
            mAppNames.push_back(appName);
        }
        sort(mAppNames.begin(), mAppNames.end());

        if (!APP_NAME.empty())
        {
            int idx = 0;
            for (auto& name : mAppNames)
            {
                if (name == APP_NAME)
                {
                    mAppId = idx;
                    break;
                }
                idx++;
            }
        }

        lines = executeAdb("shell getprop ro.build.version.release");
        if (!lines.empty())
            mDeviceStat.os_version = "Android " + lines[0];

#if 0
        lines = executeAdb("shell getprop persist.sys.rog.width");
        if (!lines.empty())
            mDeviceStat.width = stoi(lines[0]);

        lines = executeAdb("shell getprop persist.sys.rog.height");
        if (!lines.empty())
            mDeviceStat.height = stoi(lines[0]);
#endif

        lines = executeAdb("shell getprop ro.opengles.version");
        if (!lines.empty())
        {
            int version = stoi(lines[0]);
            if (version == 65535) mDeviceStat.gfx_api_version = "OpenGL ES 1.0";
            else if (version == 65536) mDeviceStat.gfx_api_version = "OpenGL ES 1.1";
            else if (version == 131072) mDeviceStat.gfx_api_version = "OpenGL ES 2.0";
            else if (version == 196608) mDeviceStat.gfx_api_version = "OpenGL ES 3.0";
            else if (version == 196609) mDeviceStat.gfx_api_version = "OpenGL ES 3.1";
            else if (version == 196610) mDeviceStat.gfx_api_version = "OpenGL ES 3.2";
        }

        //lines = executeAdb("shell getprop ro.hardware");
        //if (!lines.empty())
        //    mDeviceStat.hardware = lines[0];

        return true;
    }
    
    bool startProfiler_ios(const string& pacakgeName)
    {
        //-o=cpu,memory,fps
        char cmd[256];
        sprintf(cmd, "perf -B %s -o=fps", pacakgeName.c_str());
        auto perfResults = executeIdb(cmd);

        return true;
    }

    bool capturePerfetto()
    {
        auto ts = getTimestampForFilename();
        string name = mPackageName + "-" + ts;

        mAsyncCommands.pushFront((getAppPath() / "capture-perfetto.bat").string() + " " + name);

        return true;
    }

    void exportGpuTrace()
    {
        auto ts = getTimestampForFilename();
        string name = mAppNames[mAppId] + ts + ".gpu.json";
        JsonTree tree;

        tree.write(getAppPath() / name);
    }

    bool exportCsv()
    {
        auto ts = getTimestampForFilename();
        string name = mAppNames[mAppId] + "-" + ts + ".csv";
        FILE* fp = fopen((getAppPath() / name).string().c_str(), "w");
        if (!fp) return false;

        {
            fprintf(fp, "%s,%s\n", 
                ts.c_str(), mPackageName.c_str());
            fprintf(fp, "\n");
        }

        {
            fprintf(fp, "DeviceInfo\n");
            fprintf(fp, "Device Name,OS,OpenGL,SerialNum,CPU Info\n"
                "%s,%s,%s,%s,%s\n",
                mDeviceNames[DEVICE_ID].c_str(),
                mDeviceStat.os_version.c_str(),
                mDeviceStat.gfx_api_version.c_str(),
                mSerialNames[DEVICE_ID].c_str(),
                mDeviceStat.hardware.c_str()
            );
            fprintf(fp, "\n");
        }

        {
            fprintf(fp, "Avg(FPS),Avg(Memory)[MB],Peak(Memory)[MB]\n"
                "%.1f,%.0f,%.0f\n",
                mFpsSummary.Avg,
                mMemorySummary.Avg,
                mMemorySummary.Max);
            fprintf(fp, "\n");
        }

        {
            fprintf(fp, "Num,FPS,"
                "Memory[MB],NativePss[MB],Gfx[MB],EGL[MB],GL[MB],Unknown[MB],"
                "PrivateClean[MB],PrivateDirty[MB],"
                "CpuTemp,GpuTemp,BatteryTemp\n");

            int memory_count = min<int>(mFpsArray.size(), mMemoryStats.size());
            int fps_offset = mFpsArray.size() - memory_count;
            for (int i = 0; i < memory_count; i++)
            {
                fprintf(fp, "%d,%.1f,"
                    "%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,"
                    "%.0f,%.0f,"
                    "%.0f,%.0f,%.0f\n",
                    i, mFpsArray[fps_offset+i].second,
                    mMemoryStats[i].second.pssTotal, mMemoryStats[i].second.pssNativeHeap, mMemoryStats[i].second.pssUnknown,
                    mMemoryStats[i].second.pssGfx, mMemoryStats[i].second.pssEGL, mMemoryStats[i].second.pssGL,
                    mMemoryStats[i].second.privateClean, mMemoryStats[i].second.privateDirty,
                    mTemperatureStats[i].second.cpu, mTemperatureStats[i].second.gpu, mTemperatureStats[i].second.battery
                );
            }
            fprintf(fp, "\n");
        }

        fclose(fp);

        return true;
    }

    bool startProfiler(const string& pacakgeName)
    {
        if (mIsIOSDevices[DEVICE_ID]) return startProfiler_ios(pacakgeName);

        string perfettoCmd = perfettoCmdTemplate;
        perfettoCmd.replace(perfettoCmd.find("ATRACE_APP_NAME"), strlen("ATRACE_APP_NAME"), pacakgeName);
        ofstream ofs(getAppPath() / "p.cfg");
        if (ofs.is_open())
            ofs << perfettoCmd;

        char cmd[256];
        mSurfaceViewName = "";
        mPackageName = pacakgeName;
        pid = getPid(pacakgeName);

        mFpsSummary.Min = FLT_MAX;
        mMemorySummary.Min = FLT_MAX;
        mAppCpuSummary.Min = FLT_MAX;
        mCpuTempSummary.Min = FLT_MAX;

        auto lines = executeAdb("shell dumpsys SurfaceFlinger --list");
        for (auto& line : lines)
        {
            if (line.find("SurfaceView") != 0) continue;
            if (line.find(pacakgeName) == string::npos) continue;

            mSurfaceViewName = line;
            CI_LOG_W("Found SurfaceView:" << mSurfaceViewName);

            //executeAdb("shell dumpsys SurfaceFlinger --latency-clear");

            break;
            // TODO: support multi-surface view
        }

        mIsProfiling = true;

        return true;
    }

    void resetPerfData()
    {
        AdbResults results;
        while (mAdbResults.tryPopBack(&results))
        {
        }

        mLastSnapshotTs = 0;
        mLastSnapshotIdx = 0;
        firstCpuStatTimestamp = 0;
        firstFrameTimestamp = 0;
        mMemoryStats.clear();
        mTimestamps.clear();
        mFrameTimes.clear();
        mFpsArray.clear();
        mCpuStats.clear();
        mAppCpuStats.clear();
        for (auto& item : mChildCpuStats)
            item.clear();

        mFpsSummary.reset();
        mMemorySummary.reset();
        mAppCpuSummary.reset();
        mCpuTempSummary.reset();

        mTemperatureStats.clear();
    }

    bool stopProfiler()
    {
        mIsProfiling = false;

        resetPerfData();

        return true;
    }

    bool updateProfiler(const AdbResults& results)
    {
        {
            // frame time
            uint64_t prevMaxTimestamp = 0;
            if (!mTimestamps.empty())
                prevMaxTimestamp = mTimestamps[mTimestamps.size() - 1];

            // adb shell dumpsys SurfaceFlinger --latency <window name>
            // prints some information about the last 128 frames displayed in
            // that window.
            // The data returned looks like this:
            // 16954612
            // 7657467895508   7657482691352   7657493499756
            // 7657484466553   7657499645964   7657511077881
            // 7657500793457   7657516600576   7657527404785
            // (...)
            //
            // The first line is the refresh period (here 16.95 ms), it is followed
            // by 128 lines w/ 3 timestamps in nanosecond each:
            // A) when the app started to draw
            // B) the vsync immediately preceding SF submitting the frame to the h/w
            // C) timestamp immediately after SF submitted that frame to the h/w
            //
            // The difference between the 1st and 3rd timestamp is the frame-latency.
            // An interesting data is when the frame latency crosses a refresh period
            // boundary, this can be calculated this way:
            //
            // ceil((C - A) / refresh-period)
            //
            // (each time the number above changes, we have a "jank").
            // If this happens a lot during an animation, the animation appears
            // janky, even if it runs at 60 fps in average.
            //
            // We use the special "SurfaceView" window name because the statistics for
            // the activity's main window are not updated when the main web content is
            // composited into a SurfaceView.

            auto lines = results.SurfaceFlinger_latency;
            for (int i = 1; i < lines.size(); i++)
            {
                auto& tokens = split(lines[i], '\t');
                if (tokens.size() < 3) continue; // it happens sometimes
                auto ts = fromString<uint64_t>(tokens[2]);
                if (ts == INT64_MAX)
                {
                    // If a fence associated with a frame is still pending when we query the
                    // latency data, SurfaceFlinger gives the frame a timestamp of INT64_MAX.
                    // Since we only care about completed frames, we will ignore any timestamps
                    // with this value.
                    continue;
                }
                ts /= 1e6; // ns -> ms
                if (ts <= prevMaxTimestamp) // duplicated timestamps
                    continue;
                if (mLastSnapshotTs == 0)
                {
                    mLastSnapshotTs = ts;
                    mLastSnapshotIdx = 0;
                }
                if (ts - mLastSnapshotTs >= 1000)
                {
                    // calculate fps
                    float frameCount = (mTimestamps.size() - mLastSnapshotIdx) * 1000.0f / (ts - mLastSnapshotTs);

                    mFpsSummary.update(frameCount, mFpsArray.size());

                    mFpsArray.push_back({ ts, frameCount });

                    mLastSnapshotTs = ts;
                    mLastSnapshotIdx = mTimestamps.size();
                }

                mTimestamps.push_back(ts);
                if (mTimestamps.size() > 1)
                    mFrameTimes.push_back({ ts, ts - mTimestamps[mTimestamps.size() - 2] }); // -2 is prev item
            }

            if (firstFrameTimestamp == 0 && !mTimestamps.empty())
            {
                // TODO: a potential bug
                firstFrameTimestamp = mTimestamps[0];
                deltaTimestamp = mTimestamps[mTimestamps.size() - 1] - firstFrameTimestamp;
            }
        }

        auto lines = results.EPOCHREALTIME;
        uint64_t millisec_since_epoch = fromString<double>(lines[0]) * 1e3;

        //auto millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if (mCpuStats.empty())
            firstCpuStatTimestamp = millisec_since_epoch - deltaTimestamp - 1e3; // TODO: magic 

        {
            // CPU Usage
            auto lines = results.proc_stat;
            for (auto& line : lines)
            {
                if (line.find("cpu") == string::npos)
                    continue;

                CpuStat new_stat(line);

                if (line[3] == ' ')
                {
                    // total CPU
                    mCpuStats.push_back({ millisec_since_epoch, new_stat });
                }
                else
                {
                    // child CPU
                    int cpu_id = line[3] - '0';
                    if (cpu_id >= 0 && cpu_id < mCpuConfigs.size())
                    {
                        mChildCpuStats[cpu_id].push_back({ millisec_since_epoch, new_stat });
                    }
                }
            }

            {
                // App CPU Usage
                lines = results.proc_pid_stat;
                if (!lines.empty() && lines[0].find("No such") == string::npos)
                {
                    AppCpuStat new_stat(lines[0]);
                    mAppCpuStats.push_back({ millisec_since_epoch, new_stat });
                }
            }

            {
                // Memory Usage
                // https://perfetto.dev/docs/case-studies/memory
                lines = results.dump_sys_meminfo;
                if (lines.size() > 1)
                {
                    MemoryStat stat;
                    for (auto& line : lines)
                    {
                        if (line.find("Native Heap") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssNativeHeap = fromString<int>(tokens[2]) / 1024; // KB -> MB
                        }
                        else if (line.find("EGL mtrack") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssEGL = fromString<int>(tokens[2]) / 1024; // KB -> MB
                        }
                        else if (line.find("Gfx dev") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssGfx = fromString<int>(tokens[2]) / 1024; // KB -> MB
                        }
                        else if (line.find("GL mtrack") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssGL = fromString<int>(tokens[2]) / 1024; // KB -> MB
                        }
                        else if (line.find("Unknown") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssUnknown = fromString<int>(tokens[1]) / 1024; // KB -> MB
                        }
                        else if (line.find("TOTAL") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.pssTotal = fromString<int>(tokens[1]) / 1024; // KB -> MB

                            mMemorySummary.update(stat.pssTotal, mMemoryStats.size());

                            break;
                        }
                    }

                    lines = results.proc_pid_smaps_rollup;
                    for (auto& line : lines)
                    {
                        if (line.find("Private_Clean") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.privateClean = fromString<int>(tokens[1]) / 1024; // KB -> MB
                        }
                        else if (line.find("Private_Dirty") != string::npos)
                        {
                            auto& tokens = split(line, ' ');
                            stat.privateDirty = fromString<int>(tokens[1]) / 1024; // KB -> MB
                        }
                    }

                    mMemoryStats.push_back({ millisec_since_epoch, stat });
                }
            }

            {
                // scaling_cur_freq
                lines = results.scaling_cur_freq;
                int idx = 0;
                for (auto& line : lines)
                {
                    auto freq = stoi(line);
                    mChildCpuStats[idx].back().second.freq = freq;
                    idx++;
                }
            }

            {
                if (results.temperature.cpu > 0 || results.temperature.gpu > 0)
                {
                    if (mTemparatureStatSlot.cpu != -1)
                        mCpuTempSummary.update(results.temperature.cpu, mTemperatureStats.size());
                    mTemperatureStats.push_back({ millisec_since_epoch, results.temperature });
                }
            }
        }
        return true;
    }

    int getPid(const string& pacakgeName)
    {
        char cmd[256];
        sprintf(cmd, "shell pidof %s", pacakgeName.c_str());
        auto lines = executeAdb(cmd);
        if (lines.empty() || lines[0].empty()) // not running
            return 0;

        return fromString<int>(lines[0]);
    }

    bool startApp_ios(const string& pacakgeName)
    {
        char cmd[256];
        //if (pid = getPid(pacakgeName)) // already running
            //return true;

        sprintf(cmd, "launch %s", pacakgeName.c_str());
        executeIdb(cmd);

        return true;
    }

    bool startApp(const string& pacakgeName)
    {
        if (mIsIOSDevices[DEVICE_ID])
            return startApp_ios(pacakgeName);

        char cmd[256];
        //if (pid = getPid(pacakgeName)) // already running
            //return true;

        sprintf(cmd, "shell cmd package resolve-activity --brief %s", pacakgeName.c_str());
        auto lines = executeAdb(cmd);
        if (lines.size() == 2)
        {
            auto fullActivityName = lines[1];
            sprintf(cmd, "shell am start --activity-single-top %s", pacakgeName.c_str());
            auto startResults = executeAdb(cmd);
            if (startResults.size() > 2 && startResults[1].find("Error") == string::npos)
                return true;
        }
        sprintf(cmd, "shell monkey -p %s -v 1", pacakgeName.c_str());
        executeAdb(cmd);
        return true;
    }

    bool stopApp_ios(const string& pacakgeName)
    {
        char cmd[256];
        //if (pid = getPid(pacakgeName)) // already running
            //return true;

        sprintf(cmd, "kill %s", pacakgeName.c_str());
        executeIdb(cmd);

        return true;
    }

    bool stopApp(const string& pacakgeName)
    {
        if (mIsIOSDevices[DEVICE_ID])
        {
            stopApp_ios(pacakgeName);
        }
        else
        {
            char cmd[256];
            sprintf(cmd, "shell am force-stop %s", pacakgeName.c_str());
            auto lines = executeAdb(cmd);
        }

        stopProfiler();

        return true;
    }

    void updateMetricsData()
    {
        global_min_t = RANGE_START;
        if (!mTimestamps.empty())
            global_max_t = max<float>((mTimestamps[mTimestamps.size() - 1] - mTimestamps[0]) * 1e-3, (RANGE_START + RANGE_DURATION));
        else
            global_max_t = max<float>((mCpuStats[mCpuStats.size() - 1].first - firstCpuStatTimestamp) * 1e-3, (RANGE_START + RANGE_DURATION));

        {
            auto& metrics = storage.metric_storage["frame_time"];
            metrics.name = "frame_time";
            metrics.min_x = -1;
            metrics.max_x = 50;
        }
        {
            auto& metrics = storage.metric_storage["fps"];
            metrics.name = "fps";
            metrics.min_x = -1;
            metrics.max_x = mFpsSummary.Max + 10;
        }
        {
            auto& metrics = storage.metric_storage["cpu_usage"];
            metrics.name = "cpu_usage";
            metrics.min_x = -1;
            metrics.max_x = 101;
        }
        {
            auto& metrics = storage.metric_storage["core_usage"];
            metrics.name = "core_usage";
            metrics.min_x = -1;
            metrics.max_x = 101;
        }
        {
            auto& metrics = storage.metric_storage["memory_usage"];
            metrics.name = "memory_usage";
            metrics.min_x = 0;
            metrics.max_x = mMemorySummary.Max + 200;
        }
        {
            auto& metrics = storage.metric_storage["cpu_freq"];
            metrics.name = "cpu_freq";
            metrics.min_x = -1;
            metrics.max_x = 101;
        }
        {
            auto& metrics = storage.metric_storage["temperature"];
            metrics.name = "temperature";
            metrics.min_x = -1;
            metrics.max_x = 101;
        }
    }

    void drawDevicePanel()
    {
        if (ImGui::Button("Refresh"))
        {
            refreshDeviceNames();
            if (DEVICE_ID != -1)
                refreshDeviceDetails();
        }
        if (ImGui::Combo("Pick a device", &DEVICE_ID, mDeviceNames))
        {
        }

        if (DEVICE_ID != mDeviceId)
        {
            refreshDeviceDetails();
            resetPerfData();
            mDeviceId = DEVICE_ID;
        }

        if (DEVICE_ID != -1)
        {
            if (ImGui::Combo("Pick an app", &mAppId, mAppNames, ImGuiComboFlags_HeightLarge))
            {
                APP_NAME = mAppNames[mAppId];
            }

            ImGui::Indent();
            if (ImGui::Button("Pick Topmost App"))
            {
                // adb shell "dumpsys activity activities | grep mResumedActivity"
                auto lines = executeAdb("shell \"dumpsys activity activities | grep mResumedActivity\"");
                if (!lines.empty())
                {
                    auto tokens = split(lines[0], ": {}/");
                    auto topAppName = tokens[4];
                    if (APP_NAME != topAppName)
                    {
                        APP_NAME = topAppName;
                        stopProfiler();
                    }

                    int idx = 0;
                    for (auto& name : mAppNames)
                    {
                        if (name == APP_NAME)
                        {
                            mAppId = idx;
                            break;
                        }
                        idx++;
                    }
                }
            }
            ImGui::Unindent();

            if (mAppId != -1)
            {
                ImGui::Indent();

                if (ImGui::Button("Start App"))
                {
                    startApp(mAppNames[mAppId]);
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop App"))
                {
                    stopApp(mAppNames[mAppId]);
                }

                ImGui::Unindent();
                if (mIsProfiling)
                {
                    if (ImGui::Button("Stop Profiling"))
                    {
                        stopProfiler();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Capture Perfetto"))
                    {
                        capturePerfetto();
                    }

#ifdef MIX_DEVICE_ENABLED
                    if (ImGui::Button("Start GpuTrace"))
                    {
                        mMixDevice.startGpuTrace();
                    }
                    
                    ImGui::SameLine();

                    if (ImGui::Button("Stop GpuTrace"))
                    {
                        mMixDevice.stopGpuTrace();
                        exportGpuTrace();
                    }
#endif

                    if (ImGui::Button("Export"))
                    {
                        exportCsv();
                    }
                }
                else
                {
                    if (mAutoStart)
                    {
                        startProfiler(mAppNames[mAppId]);
                        mAutoStart = false;
                    }
                    if (ImGui::Button("Start Profiling"))
                    {
                        startProfiler(mAppNames[mAppId]);
                    }
                }
            }

            if (ImGui::CollapsingHeader("Config", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                if (!mDeviceStat.os_version.empty())
                    ImGui::Text(mDeviceStat.os_version.c_str());
                if (!mDeviceStat.hardware.empty())
                    ImGui::Text("%s", mDeviceStat.hardware.c_str());
                if (mDeviceStat.width > 0 && mDeviceStat.height > 0)
                    ImGui::Text("%d x %d", mDeviceStat.width, mDeviceStat.height);
                if (!mDeviceStat.gfx_api_version.empty())
                    ImGui::Text(mDeviceStat.gfx_api_version.c_str());
                for (const auto& config : mCpuConfigs)
                {
                    if (config.cpuinfo_min_freq > 0)
                    {
                        ImGui::Text("cpu_%d: %s %.2f~%.2f MHz",
                            config.id, config.part.c_str(),
                            config.cpuinfo_min_freq / 1e6, config.cpuinfo_max_freq / 1e6);
                    }
                    else
                    {
                        ImGui::Text("cpu_%d: %s",
                            config.id, config.part.c_str());
                    }
                }
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Charts", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto& kv : storage.metric_storage)
                {
                    ImGui::Checkbox(kv.first.c_str(), &kv.second.visible);
                }
            }
        }
    }

    void drawPerfPanel()
    {
        for (const auto& kv : storage.span_storage)
        {
            auto& series = kv.second;

            ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
            ImPlot::SetNextPlotLimitsX(global_min_t, global_max_t, ImGuiCond_Always);
            ImPlot::SetNextPlotTicksY(0, 10, 2);

            if (ImPlot::BeginPlot(series.name.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT), ImPlotFlags_NoChild, ImPlotAxisFlags_None))
            {
                //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
                ImPlot::PlotRects(series.name.c_str(), SpanSeries::getter, (void*)&series, series.span_array.size() * 2);
                for (const auto& span : series.span_array)
                {
                    ImPlot::PlotText(span.name.c_str(), span.start/*  / TIME_UNIT_SCALE */, 0, false, ImVec2(0, -PANEL_HEIGHT / 2));
                }

#if 0
                ImPlot::PushPlotClipRect();
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(0, 255, 255, 64));
                ImPlot::PopPlotClipRect();
#endif
                if (SHOW_TOOL_TIP && ImPlot::IsPlotHovered())
                {
                    ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                    mouse.x = round(mouse.x);
                    float half_width = 10;
                    float  tool_l = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
                    float  tool_r = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
                    float  tool_t = ImPlot::GetPlotPos().y;
                    float  tool_b = tool_t + ImPlot::GetPlotSize().y;
                    for (const auto& span : series.span_array)
                    {
                        if (span.start <= mouse.x && mouse.x < span.end)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text(span.label.c_str());
                            ImGui::EndTooltip();
                            break;
                        }
                    }
                }

                //ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }
        }

        for (const auto& kv : storage.metric_storage)
        {
            auto& series = kv.second;
            if (!series.visible) continue;

            //ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
            ImPlot::SetNextPlotLimitsX(global_min_t, global_max_t, ImGuiCond_Always);
            //ImPlot::SetNextPlotTicksY(series.min_x, series.max_x, PANEL_TICK_X);
            ImPlot::SetNextPlotLimitsY(series.min_x, series.max_x, ImGuiCond_Always);

            string title = series.name;
            char text[256];

            if (series.name == "fps" && !mFpsArray.empty())
            {
                sprintf(text, "fps [%.1f, %.1f] avg: %.1f", mFpsSummary.Min, mFpsSummary.Max, mFpsSummary.Avg);
                title = text;
            }
            if (series.name == "memory_usage" && !mMemoryStats.empty())
            {
                sprintf(text, "memory_usage [%.0f, %.0f] avg: %.0f", mMemorySummary.Min, mMemorySummary.Max, mMemorySummary.Avg);
                title = text;
            }
            if (series.name == "temperature" && !mTemperatureStats.empty())
            {
                sprintf(text, "temperature [%.0f, %.0f] avg: %.0f", mCpuTempSummary.Min, mCpuTempSummary.Max, mCpuTempSummary.Avg);
                title = text;
            }
            if (series.name == "cpu_usage" && !mAppCpuStats.empty())
            {
                sprintf(text, "cpu_usage [%.0f, %.0f] avg: %.1f", mAppCpuSummary.Min, mAppCpuSummary.Max, mAppCpuSummary.Avg);
                //title = text;
                // TODO: fix bug
            }
            if (ImPlot::BeginPlot(title.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT), ImPlotFlags_NoChild, ImPlotAxisFlags_None))
            {
                ImPlot::SetLegendLocation(ImPlotLocation_North | ImPlotLocation_West);

                //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
                if (series.name == "frame_time")
                {
                    ImPlot::PlotLineG(series.name.c_str(), frameTime_getter, (void*)&mFrameTimes, mFrameTimes.size());
                }
                else if (series.name == "fps")
                {
                    ImPlot::PlotLineG(series.name.c_str(), fps_getter, (void*)&mFpsArray, mFpsArray.size());
                }
                else if (series.name == "cpu_usage")
                {
                    ImPlot::PlotLineG("sys", cpuUsage_getter, (void*)&mCpuStats, mCpuStats.size() - 1);
                    ImPlot::PlotLineG("app", app_cpuUsage_getter, (void*)&mAppCpuStats, mAppCpuStats.size() - 1);
                }
                else if (series.name == "core_usage")
                {
                    char label[] = "cpu_0";
                    for (int i = 0; i < mCpuConfigs.size(); i++)
                    {
                        label[4] = '0' + i;
                        ImPlot::PlotLineG(label, cpuUsage_getter, (void*)&mChildCpuStats[i], mChildCpuStats[i].size() - 1);
                    }
                }
                else if (series.name == "cpu_freq")
                {
                    char label[] = "cpu_0";
                    for (int i = 0; i < mCpuConfigs.size(); i++)
                    {
                        label[4] = '0' + i;
                        ImPlot::PlotLineG(label, cpuFreq_getter, (void*)&mChildCpuStats[i], mChildCpuStats[i].size());
                    }
                }
                else if (series.name == "memory_usage")
                {
                    ImPlot::PlotLineG("total", memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                    ImPlot::PlotLineG("native_heap", nativeheap_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                    ImPlot::PlotLineG("graphics", gl_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                    ImPlot::PlotLineG("unknown", unknown_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                    ImPlot::PlotLineG("private_clean", privateClean_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                    ImPlot::PlotLineG("private_dirty", privateDirty_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                }
                else if (series.name == "temperature")
                {
                    if (mTemparatureStatSlot.cpu != -1)
                        ImPlot::PlotLineG("cpu", temp_cpu_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
                    if (mTemparatureStatSlot.gpu != -1)
                        ImPlot::PlotLineG("gpu", temp_gpu_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
                    if (mTemparatureStatSlot.battery != -1)
                        ImPlot::PlotLineG("battery", temp_battery_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
                }
                else
                    ImPlot::PlotLineG(series.name.c_str(), MetricSeries::getter, (void*)&series, series.t_array.size());
                //ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }
        }
    }

    void getMemReport()
    {
        char str[256];
        sprintf(str, "shell ls -l /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports", APP_FOLDER.c_str(), APP_FOLDER.c_str());
        auto lines = executeAdb(str);
        if (lines.size() > 1)
        {
            auto folder = lines[lines.size() - 1];;
            auto tokens = split(folder, ' ');
            folder = tokens[tokens.size() - 1];

            sprintf(str, "shell ls -l /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports/%s", APP_FOLDER.c_str(), APP_FOLDER.c_str(), folder.c_str());
            lines = executeAdb(str);
            if (lines.size() > 1)
            {
                auto lastLine = lines[lines.size() - 1];
                tokens = split(lastLine, ' ');
                auto lastFile = tokens[tokens.size() - 1];
                sprintf(str, "pull /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports/%s/%s", APP_FOLDER.c_str(), APP_FOLDER.c_str(), folder.c_str(), lastFile.c_str());
                executeAdb(str);
                launchWebBrowser(Url(lastFile, true));
            }
        }
    }

    void setup() override
    {
        SetUnhandledExceptionFilter(unhandled_handler);
        am::addAssetDirectory(getAppPath());
        mAutoStart = AUTO_START;
        ::SetCurrentDirectoryA(getAppPath().string().c_str());

        mMixDevice.setup();

        std::ifstream file(getAppPath() / "unreal-cmd.txt");
        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                mUnrealCmds.push_back(line);
            }
        }

        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
        createConfigImgui(getWindow(), false);
        implotCtx = ImPlot::CreateContext();
        ImPlot::SetColormap(ImPlotColormap_Cool);

        ImPlot::GetStyle().AntiAliasedLines = true;
        ImPlot::GetStyle().Marker = ImPlotMarker_Circle;
        ImPlot::GetStyle().MarkerSize = 2;

            //ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 5, ImVec4(1, 0.5f, 0, 0.25f));


        mAdbThread = make_unique<thread>([this] {
            static auto lastTimestamp = getElapsedSeconds();
            while (mIsRunning)
            {
                string asyncCmd;
                if (mAsyncCommands.tryPopBack(&asyncCmd))
                {
                    string output;
                    runCmd(asyncCmd, output);
                    if (asyncCmd.find("perfetto") != string::npos)
                    {
                        auto cmds = split(asyncCmd, " ");
                        dispatchAsync([this, cmds] {
                            launchWebBrowser(Url("https://ui.perfetto.dev/#!/", true));
                            goto_folder(getAppPath(), getAppPath() / (cmds[1] + ".perfetto"));
                        });
                    }
                }

                if (!mIsProfiling || getElapsedSeconds() - lastTimestamp < REFRESH_SECONDS)
                {
                    sleep(1);
                    continue;
                }
                lastTimestamp = getElapsedSeconds();

                AdbResults results;
                char cmd[256];
                if (!mSurfaceViewName.empty())
                {
                    sprintf(cmd, "shell dumpsys SurfaceFlinger --latency \\\"%s\\\"", mSurfaceViewName.c_str());
                    results.SurfaceFlinger_latency = executeAdb(cmd);
                }
                results.EPOCHREALTIME = executeAdb("shell echo $EPOCHREALTIME");
                results.proc_stat = executeAdb("shell cat /proc/stat");

                if (storage.metric_storage["memory_usage"].visible)
                {
                    sprintf(cmd, "shell dumpsys meminfo %s", mPackageName.c_str());
                    results.dump_sys_meminfo = executeAdb(cmd);

                    sprintf(cmd, "shell cat /proc/%d/smaps_rollup", pid);
                    results.proc_pid_smaps_rollup = executeAdb(cmd);
                }

                if (storage.metric_storage["cpu_freq"].visible)
                {
                    results.scaling_cur_freq = executeAdb("shell cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq");
                }

                sprintf(cmd, "shell cat /proc/%d/stat", pid);
                results.proc_pid_stat = executeAdb(cmd);
                if (!results.proc_pid_stat.empty() && results.proc_pid_stat[0].find("error") != string::npos)
                {
                    results.success = false;
                }

                {
                    if (mTemparatureStatSlot.cpu != -1)
                    {
                        sprintf(cmd, "shell cat /sys/devices/virtual/thermal/thermal_zone%d/temp", mTemparatureStatSlot.cpu);
                        auto lines = executeAdb(cmd);
                        if (!lines.empty()) results.temperature.cpu = stoi(lines[0]) * 1e-3;
                    }
                    if (mTemparatureStatSlot.gpu != -1)
                    {
                        sprintf(cmd, "shell cat /sys/devices/virtual/thermal/thermal_zone%d/temp", mTemparatureStatSlot.gpu);
                        auto lines = executeAdb(cmd);
                        if (!lines.empty()) results.temperature.gpu = stoi(lines[0]) * 1e-3;
                    }
                    if (mTemparatureStatSlot.battery != -1)
                    {
                        sprintf(cmd, "shell cat /sys/devices/virtual/thermal/thermal_zone%d/temp", mTemparatureStatSlot.battery);
                        auto lines = executeAdb(cmd);
                        if (!lines.empty()) results.temperature.battery = stoi(lines[0]) * 1e-3;
                    }
                }

                mAdbResults.pushFront(results);
            }
        });

        refreshDeviceNames();

        mLastUpdateTime = getElapsedSeconds();

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getWindow()->getSignalClose().connect([&] {
            mIsRunning = false;
            mAdbThread->join();
        });

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getSignalUpdate().connect([&] {
#ifndef NDEBUG
            bool open = false;
            ImPlot::ShowDemoWindow(&open);
#endif
            if (ImGui::Begin("Devices", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
                if (ImGui::BeginTabBar("DeviceTab", tab_bar_flags))
                {
                    if (ImGui::BeginTabItem("Devices"))
                    {
                        drawDevicePanel();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Settings"))
                    {
                        ImPlot::ShowStyleSelector("Style");
                        ImPlot::ShowColormapSelector("Colormap");
                        vnm::drawMinicofigImgui();

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Unreal"))
                    {
                        char str[256];

                        ImGui::InputText("APP_FOLDER", &APP_FOLDER);
                        if (ImGui::Button("Get log"))
                        {
                            sprintf(str, "pull /sdcard/UE4Game/%s/%s/Saved/Logs/%s.log", APP_FOLDER.c_str(), APP_FOLDER.c_str(), APP_FOLDER.c_str());
                            executeAdb(str);
                            launchWebBrowser(Url(APP_FOLDER + ".log", true));
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Get memreport"))
                        {
                            getMemReport();
                        }

                        ImGui::InputText(" ", &UE_CMD);
                        ImGui::SameLine();
                        if (ImGui::Button("Apply"))
                        {
                            sprintf(str, "shell am broadcast -a android.intent.action.RUN -e cmd '%s'", UE_CMD.c_str());
                            executeAdb(str);
                            if (UE_CMD.find("memreport") != string::npos)
                                getMemReport();
                        }
                        ImGui::NewLine();

                        for (const auto cmd : mUnrealCmds)
                        {
                            if (cmd.empty())
                            {
                                ImGui::NewLine();
                                continue;
                            }

                            if (cmd[0] == '/') continue;

                            auto tokens = split(cmd, '|');
                            for (int i = 0; i < tokens.size(); i++)
                            {
                                if (i > 0)
                                    ImGui::SameLine();

                                auto& token = tokens[i];
                                if (ImGui::Button(token.c_str()))
                                {
                                    UE_CMD = token;
                                    sprintf(str, "shell am broadcast -a android.intent.action.RUN -e cmd '%s'", token.c_str());
                                    executeAdb(str);

                                    if (token.find("memreport") != string::npos)
                                    {
                                        getMemReport();
                                    }
                                }
                            }
                        }

                        ImGui::EndTabItem();
                    }

                }
                ImGui::EndTabBar();

                ImGui::End();
            }

            if (mIsProfiling)
            {
                AdbResults results;
                if (mAdbResults.tryPopBack(&results))
                {
                    if (results.success)
                    {
                        updateProfiler(results);
                        updateMetricsData();
                    }
                }
            }

            if (ImGui::Begin("Performance"))
            {
                drawPerfPanel();
            }
            ImGui::End();

        });

        getSignalCleanup().connect([&] {
            ImPlot::DestroyContext(implotCtx);
            writeConfig();
        });

        getWindow()->getSignalDraw().connect([&] {
            gl::clear(ColorA::gray(BACKGROUND_GRAY));
        });
    }
};

CINDER_APP(PerfDoctorApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
