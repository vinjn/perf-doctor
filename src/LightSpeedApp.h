#define _HAS_STD_BYTE 0

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/ConcurrentCircularBuffer.h"

#ifdef MIX_DEVICE_ENABLED
#include "MixDevice.h"
#else
struct MixDevice
{
    void setup()
    {
    }
};
#endif

#include "AssetManager.h"
#include "implot/implot.h"
#include "implot/implot_internal.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct Span
{
    string name;
    string label;
    float start = 0;
    float end = 0;
};

struct LabelPair
{
    string name;
    uint64_t start = 0;
    uint64_t end = 0;
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
    string cpu;
    string gpu;
    string battery;
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
    int fps_min = 0, fps_max = 0, fps_now = 0;
    string display_WxH;
    string os_version;
    string gfx_api_version;
    string market_name; // iOS only
    string hardware;
    string gpu_name;
};

struct CpuStat
{
    int cpu_id;
    long int user, nice, sys, idle, iowait, irq, softirq;
    int freq = -1;

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

float calcCpuUsage(const CpuStat& lhs, const CpuStat& rhs);

struct AppCpuStat
{
    // https://www.chenwenguan.com/android-performance-monitor-cpu/
    long int utime, stime;
    long int cutime, cstime;

    AppCpuStat(const string& line);

    long int getActiveTime() const
    {
        return utime + stime + cutime + cstime;
    }
};

float calcAppCpuUsage(const CpuStat& lhs, const CpuStat& rhs, const AppCpuStat& appLhs, const AppCpuStat& appRhs);

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

// TODO: so ugly
extern float global_min_t;
extern float global_max_t;
extern uint64_t firstFrameTimestamp;
extern uint64_t deltaTimestamp;
extern uint64_t firstCpuStatTimestamp;
extern int pid;
extern vector<pair<uint64_t, CpuStat>> mCpuStats;
extern vector<CpuConfig> mCpuConfigs;

int runCmd(const string& cmd, std::string& outOutput, bool waitForCompletion = true);

struct AdbResults
{
    bool success = true;
    vector<string> SurfaceFlinger_latency;
    vector<string> dumpsys_gfxinfo;
    vector<string> EPOCHREALTIME;
    vector<string> proc_stat;
    vector<string> proc_pid_stat;
    vector<string> dumpsys_meminfo;
    vector<string> scaling_cur_freq;
    TemperatureStat temperature;
};

struct TickFunction
{
    string actor;
    string status;
    string tick_group;

    string type;
    string component;
    string level;
    //vector<string> prerequesities;
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
    vector<string> executeIdb(string cmd, bool async = false, bool oneDeviceOnly = true);

    vector<string> executeAdb(string cmd, bool oneDeviceOnly = true);
    void executeUnrealCmd(const string& cmd);

    int mAppId = -1;
    vector<string> mSerialNames;
    vector<string> mDeviceNames;
    vector<bool> mIsIOSDevices;
    vector<string> mAppNames;
    string mSurfaceViewName = "";
    string mSurfaceResolution = "";
    string mPackageName = "";
    bool mIsProfiling = false;
    float mLastUpdateTime = 0;
    vector<pair<uint64_t, uint64_t>> mFrameTimes;
    vector<uint64_t> mTimestamps; //ms
    vector<pair<uint64_t, float>> mFpsArray;
    vector<pair<uint64_t, AppCpuStat>> mAppCpuStats;
    vector<pair<uint64_t, CpuStat>> mChildCpuStats[8];
    vector<pair<uint64_t, MemoryStat>> mMemoryStats;
    vector<LabelPair> mLabelPairs;

    TemperatureStatSlot mTemparatureStatSlot;
    vector<pair<uint64_t, TemperatureStat>> mTemperatureStats;

    vector<string> mUnrealCmds;

    vector<TickFunction> mTickFunctions;
    
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

    MetricSummary mFpsSummary, mMemorySummary, mAppCpuSummary, mCpuTempSummary, mFrameTimeSummary;
    int mDeviceId = -1;

    bool refreshDeviceNames();

    bool refreshDeviceDetails_ios();

    bool refreshDeviceDetails();
    
    bool startProfiler_ios(const string& pacakgeName);

    bool capturePerfetto();

    bool captureSimpleperf();

    bool screenshot();

    void exportGpuTrace();

    bool exportCsv();

    void trimMemory(const char* level);

    bool startProfiler(const string& pacakgeName);

    void resetPerfData();

    bool stopProfiler();

    struct TripleTimestamp
    {
        string started_to_draw;
        string vsync;
        string frame_submitted;
    };

    bool updateProfiler(const AdbResults& results);

    int getPid(const string& pacakgeName);

    bool startApp_ios(const string& pacakgeName);

    bool startApp(const string& pacakgeName);

    bool stopApp_ios(const string& pacakgeName);

    bool stopApp(const string& pacakgeName);

    void updateMetricsData();

    void drawLeftSidePanel();
    void drawDeviceTab();
    void drawPerfPanel();
    void drawLabel();

    void getUnrealLog(bool openLogFile = false);
    void getMemReport();
    void getDumpTicks();

    void setup() override;
};
