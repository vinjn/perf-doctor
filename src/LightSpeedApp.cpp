#include "LightSpeedApp.h"
#include "MiniConfig.h"
#include "Cinder/Timeline.h"
#include "cinder/Json.h"

// TODO: so ugly
float global_min_t = 0;
float global_max_t = 1;
uint64_t firstFrameTimestamp = 0;
uint64_t deltaTimestamp = 0;
uint64_t firstCpuStatTimestamp = 0;
int pid;
vector<pair<uint64_t, CpuStat>> mCpuStats;
vector<CpuConfig> mCpuConfigs;

AppCpuStat::AppCpuStat(const string& line)
{
    auto tokens = split(line, ' ');
    utime = fromString<long int>(tokens[14]);
    stime = fromString<long int>(tokens[15]);
    cutime = fromString<long int>(tokens[16]);
    cstime = fromString<long int>(tokens[17]);
}

string getTimestampForFilename()
{
    char buffer[256];
    time_t rawtime;
    time(&rawtime);
    tm* timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y_%b_%d-%H_%M_%S ", timeinfo);

    return string(buffer);
}

string perfettoCmdTemplate;

float calcCpuUsage(const CpuStat& lhs, const CpuStat& rhs)
{
    auto totalTime = rhs.getAll() - lhs.getAll();
    auto idleTime = rhs.getIdle() - lhs.getIdle();
    auto usage = (totalTime - idleTime) * 100.0f / totalTime;
    return usage;
}

float calcAppCpuUsage(const CpuStat& lhs, const CpuStat& rhs, const AppCpuStat& appLhs, const AppCpuStat& appRhs)
{
    auto totalTime = rhs.getAll() - lhs.getAll();
    auto appActiveTime = appRhs.getActiveTime() - appLhs.getActiveTime();
    auto usage = appActiveTime * mCpuConfigs.size() * 100.0f / totalTime;
    return usage;
}


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
            SHParseDisplayName(selectPath.wstring().c_str(), NULL, &pidlist, SFGAO_CANCOPY, &aog);//pidlist����ת���Ժ��shell·��

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

int runCmd(const string& cmd, std::string& outOutput, bool waitForCompletion)
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

    if (waitForCompletion)
    {
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
    }

    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_ERR_Rd);

    CI_LOG_W(outOutput);

    // The remaining open handles are cleaned up when this process terminates.
    // To avoid resource leaks in a larger application,
    // close handles explicitly.
    return 0;
}


vector<string> PerfDoctorApp::executeIdb(string cmd, bool async, bool oneDeviceOnly)
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

void PerfDoctorApp::executeUnrealCmd(const string& cmd)
{
    char str[256];
    sprintf(str, "shell am broadcast -a android.intent.action.RUN -e cmd '%s'", cmd.c_str());
    executeAdb(str);
    if (cmd.find("memreport") != string::npos)
        getMemReport();
    if (cmd.find("dumpticks") != string::npos)
        getDumpTicks();
}

vector<string> PerfDoctorApp::executeAdb(string cmd, bool oneDeviceOnly)
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


bool PerfDoctorApp::refreshDeviceNames()
{
    mSerialNames.clear();
    mDeviceNames.clear();
    mTemparatureStatSlot = { "","", "" };

    char cmd[256];

    auto adbDeviceResults = executeAdb("devices", false);

    for (int i = 1; i < adbDeviceResults.size(); i++)
    {
        auto tokens = split(adbDeviceResults[i], '\t');
        if (tokens.size() < 2) continue;

        if (tokens[1] != "device")
        {
#if 1
            ::MessageBoxA(NULL, "Device is unauthorized", tokens[0].c_str(), MB_OK);
#else
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
#endif
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

bool PerfDoctorApp::refreshDeviceDetails_ios()
{
    storage.metric_storage["frame_time"].visible = false;
    storage.metric_storage["cpu_usage"].visible = false;
    storage.metric_storage["core_usage"].visible = false;
    storage.metric_storage["memory_usage"].visible = false;
    storage.metric_storage["core_freq"].visible = false;
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

bool PerfDoctorApp::refreshDeviceDetails()
{
    mAppNames.clear();
    mIsProfiling = false;
    mAppId = -1;

    mCpuConfigs.clear();

    if (DEVICE_ID == -1) return true;

    storage.metric_storage["fps"].visible = fps_visible;

    if (mIsIOSDevices[DEVICE_ID]) return refreshDeviceDetails_ios();

    storage.metric_storage["frame_time"].visible = frame_time_visible;
    storage.metric_storage["cpu_usage"].visible = cpu_usage_visible;
    storage.metric_storage["core_usage"].visible = core_usage_visible;
    storage.metric_storage["memory_usage"].visible = memory_usage_visible;
    storage.metric_storage["core_freq"].visible = core_freq_visible;
    storage.metric_storage["temperature"].visible = temperature_visible;

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
        // gpu name
        lines = executeAdb("shell \"dumpsys SurfaceFlinger | grep GLES\"");
        if (!lines.empty())
        {
            auto tokens = split(lines[0], ',');
            if (tokens.size() > 2)
                mDeviceStat.gpu_name = tokens[1];
        }
    }

    {
        auto thermalFiles = executeAdb("shell ls /sys/devices/virtual/thermal/thermal_zone*/temp");

        auto lines = executeAdb("shell cat /sys/devices/virtual/thermal/thermal_zone*/type");
        for (int i = 0; i < lines.size(); i++)
        {
            if (mTemparatureStatSlot.cpu.empty() && (
                lines[i].find("cpuss-") != string::npos
                || lines[i].find("soc_thermal") != string::npos
                ))
                mTemparatureStatSlot.cpu = thermalFiles[i];
            if (mTemparatureStatSlot.gpu.empty() && lines[i].find("gpuss-") != string::npos)
                mTemparatureStatSlot.gpu = thermalFiles[i];

            if (mTemparatureStatSlot.battery.empty() && (
                lines[i].find("battery") != string::npos
                || lines[i].find("Battery") != string::npos
                ))
                mTemparatureStatSlot.battery = thermalFiles[i];
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

    lines = executeAdb("shell \"dumpsys SurfaceFlinger | grep cur:\"");
    if (!lines.empty())
    {
        auto tokens = split(lines[0], ": ");
        mDeviceStat.fps_min = fromString<int>(tokens[2]);
        mDeviceStat.fps_max = fromString<int>(tokens[4]);
        mDeviceStat.fps_now = fromString<int>(tokens[6]);
    }

    lines = executeAdb("shell \"dumpsys SurfaceFlinger | grep WxH\"");
    if (!lines.empty())
    {
        auto tokens = split(lines[0], ": ");
        mDeviceStat.display_WxH = tokens[2];
    }

    return true;
}

bool PerfDoctorApp::startProfiler_ios(const string& pacakgeName)
{
    //-o=cpu,memory,fps
    char cmd[256];
    sprintf(cmd, "perf -B %s -o=fps", pacakgeName.c_str());
    auto perfResults = executeIdb(cmd);

    return true;
}

bool PerfDoctorApp::capturePerfetto()
{
    if (!fs::exists(getAppPath() / "p.cfg"))
    {
        // create a placeholder one if missing
        ofstream ofs(getAppPath() / "p.cfg");
        if (ofs.is_open())
            ofs << perfettoCmdTemplate;
    }

    auto ts = getTimestampForFilename();
    string name = APP_NAME + "-" + ts;
    mAsyncCommands.pushFront((getAppPath() / "capture-perfetto.bat").string() + " " + name);
    mAsyncCommands.pushFront((getAppPath() / "screenshot.bat").string() + " " + name + " hide_screenshot");
    return true;
}

bool PerfDoctorApp::captureSimpleperf()
{
    auto ts = getTimestampForFilename();

    mAsyncCommands.pushFront((getAppPath() / ".." / "SimplePerf" / "run.bat").string() + " " + APP_NAME);
    return true;
}


bool PerfDoctorApp::screenshot()
{
    auto ts = getTimestampForFilename();
    string name = APP_NAME + "-" + ts;
    mAsyncCommands.pushFront((getAppPath() / "screenshot.bat").string() + " " + name);
    return true;
}

void PerfDoctorApp::exportGpuTrace()
{
    auto ts = getTimestampForFilename();
    string name = mAppNames[mAppId] + ts + ".gpu.json";
    JsonTree tree;

    tree.write(getAppPath() / name);
}

bool PerfDoctorApp::exportCsv()
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
        fprintf(fp, "Device Name,OS,OpenGL,SerialNum,CPU Info, GPU\n"
            "%s,%s,%s,%s,%s,%s\n",
            mDeviceNames[DEVICE_ID].c_str(),
            mDeviceStat.os_version.c_str(),
            mDeviceStat.gfx_api_version.c_str(),
            mSerialNames[DEVICE_ID].c_str(),
            mDeviceStat.hardware.c_str(),
            mDeviceStat.gpu_name.c_str()
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
        fprintf(fp, "Num,FPS,");

        if (storage.metric_storage["memory_usage"].visible)
        {
            fprintf(fp, "Memory[MB],NativePss[MB],"
                "Gfx[MB],EGL[MB],GL[MB],Unknown[MB],"
                "PrivateClean[MB],PrivateDirty[MB],");
        }
        if (storage.metric_storage["temperature"].visible)
            fprintf(fp, "CpuTemp,GpuTemp,BatteryTemp,");

        if (storage.metric_storage["core_freq"].visible)
        {
            for (int i = 0; i < mCpuConfigs.size(); i++)
                fprintf(fp, "CPUClock%d[MHz],", i);
        }
        if (storage.metric_storage["core_usage"].visible)
        {
            for (int i = 0; i < mCpuConfigs.size(); i++)
                fprintf(fp, "CPUUsage%d[%%],", i);
        }
        fprintf(fp, "\n");

        int memory_count = min<int>(mFpsArray.size(), mMemoryStats.size());
        int fps_offset = mFpsArray.size() - memory_count;
        for (int i = 0; i < memory_count - 1; i++)
        {
            const auto& memStat = mMemoryStats[i].second;
            fprintf(fp, "%d,%.1f,",
                i, mFpsArray[fps_offset + i].second);
            if (storage.metric_storage["memory_usage"].visible)
            {
                fprintf(fp, "%.0f,%.0f,"
                    "%.0f,%.0f,%.0f,%.0f,"
                    "%.0f,%.0f,",
                    memStat.pssTotal, memStat.pssNativeHeap,
                    memStat.pssGfx, memStat.pssEGL, memStat.pssGL, memStat.pssUnknown,
                    memStat.privateClean, memStat.privateDirty);
            }

            if (storage.metric_storage["temperature"].visible)
            {
                if (!mTemperatureStats.empty())
                {
                    fprintf(fp, "%.1f,%.1f,%.1f,",
                        max<float>(mTemperatureStats[i].second.cpu, 0),
                        max<float>(mTemperatureStats[i].second.gpu, 0),
                        max<float>(mTemperatureStats[i].second.battery, 0));
                }
                else
                {
                    fprintf(fp, "0,0,0,");
                }
            }

            if (storage.metric_storage["core_freq"].visible)
            {
                for (int k = 0; k < mCpuConfigs.size(); k++)
                {
                    if (i < mChildCpuStats[k].size())
                        fprintf(fp, "%.0f,", max<float>(mChildCpuStats[k][i].second.freq * 1e-3, 0));
                    else
                        fprintf(fp, "0,");
                }
            }
            if (storage.metric_storage["core_usage"].visible)
            {
                for (int k = 0; k < mCpuConfigs.size(); k++)
                {
                    if (i < mChildCpuStats[k].size())
                        fprintf(fp, "%.0f,", calcCpuUsage(mChildCpuStats[k][i].second, mChildCpuStats[k][i + 1].second));
                    else
                        fprintf(fp, "0,");
                }
            }

            fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

    return true;
}

void PerfDoctorApp::trimMemory(const char* level)
{
    char cmd[256];
    sprintf(cmd, "shell am send-trim-memory %s %s", mAppNames[mAppId].c_str(), level);
    auto lines = executeAdb(cmd);
}

bool PerfDoctorApp::startProfiler(const string& pacakgeName)
{
    if (mIsIOSDevices[DEVICE_ID]) return startProfiler_ios(pacakgeName);

    string perfettoCmd = perfettoCmdTemplate;
    perfettoCmd.replace(perfettoCmd.find("ATRACE_APP_NAME"), strlen("ATRACE_APP_NAME"), pacakgeName);
    ofstream ofs(getAppPath() / "p.cfg");
    if (ofs.is_open())
        ofs << perfettoCmd;

    char cmd[256];
    mSurfaceViewName = "";
    mSurfaceResolution = "";
    mPackageName = pacakgeName;
    pid = getPid(pacakgeName);

    mFpsSummary.Min = FLT_MAX;
    mMemorySummary.Min = FLT_MAX;
    mAppCpuSummary.Min = FLT_MAX;
    mCpuTempSummary.Min = FLT_MAX;
    mFrameTimeSummary.Min = FLT_MAX;

    auto lines = executeAdb("shell dumpsys SurfaceFlinger --list");
    for (auto& line : lines)
    {
        if (line.find("SurfaceView") != 0) continue;
        if (line.find(pacakgeName) == string::npos) continue;

        if (mSurfaceViewName.empty() || line.find("BLAST") != 0)
            mSurfaceViewName = line;
        CI_LOG_W("Found SurfaceView:" << mSurfaceViewName);

        //executeAdb("shell dumpsys SurfaceFlinger --latency-clear");

        //break;
        // TODO: support multi-surface view
    }

    if (!mSurfaceViewName.empty())
    {
        sprintf(cmd, "shell \"dumpsys SurfaceFlinger | grep \'%s\'\"", mSurfaceViewName.c_str());
        lines = executeAdb(cmd);
        for (auto& line : lines)
        {
            // 0x7215e95c50: 4680.00 KiB |  720 ( 768) x 1560 |    1 |        2 | 0x10000900 | SurfaceView - com.xx.yy/com.epicgames.ue4.GameActivity#0
            auto tokens = split(line, '|');
            if (tokens.size() == 6)
            {
                mSurfaceResolution = trim(tokens[1]);
            }
        }
    }

    mIsProfiling = true;

    return true;
}

void PerfDoctorApp::resetPerfData()
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
    mFrameTimeSummary.reset();

    mTemperatureStats.clear();

    mLabelPairs.clear();
}

bool PerfDoctorApp::stopProfiler()
{
    mIsProfiling = false;

    resetPerfData();

    return true;
}

bool PerfDoctorApp::updateProfiler(const AdbResults& results)
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

        vector<TripleTimestamp> timestamps;

        auto lines = results.SurfaceFlinger_latency;
        for (int i = 1; i < lines.size(); i++)
        {
            auto& tokens = split(lines[i], '\t');
            if (tokens.size() < 3) continue; // it happens sometimes
            if (tokens[0][0] == '0') continue;
            timestamps.push_back({ tokens[0], tokens[1], tokens[2] });
        }

        if (lines.empty())
        {
            // https://developer.android.com/training/testing/performance
            lines = results.dumpsys_gfxinfo;
            bool find_app = false;
            bool find_timestamps = false;
            for (int i = 0; i < lines.size(); i++)
            {
                auto& line = lines[i];
                if (find_timestamps)
                {
                    if (line.find("---PROFILEDATA---") != string::npos)
                        // reach end of timestamp section
                        break;

                    auto& tokens = split(line, ',');
                    if (fromString<uint64_t>(tokens[0]) != 0)
                        // this frame is an outlier, skip it
                        continue;

                    // ��ȡINTENDED_VSYNC VSYNC FRAME_COMPLETEDʱ�� ����VSYNC����fps jank
                    timestamps.push_back({ tokens[1], tokens[2], tokens[13] });
                }
                if (line.find(mPackageName) != string::npos)
                    find_app = true;
                if (find_app && line.find("Flags,IntendedVsync") != string::npos)
                    find_timestamps = true;
            }
        }

        for (const auto& triple : timestamps)
        {
            auto ts = fromString<uint64_t>(triple.frame_submitted);
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

            prevMaxTimestamp = mTimestamps[mTimestamps.size() - 1];
            if (mTimestamps.size() > 1)
            {
                auto frametime = ts - mTimestamps[mTimestamps.size() - 2];
                mFrameTimeSummary.update(frametime, mFrameTimes.size());
                mFrameTimes.push_back({ ts, frametime }); // -2 is prev item
            }
        }

        if (firstFrameTimestamp == 0 && !mTimestamps.empty())
        {
            // TODO: a potential bug
            firstFrameTimestamp = mTimestamps[0];
            deltaTimestamp = mTimestamps[mTimestamps.size() - 1] - firstFrameTimestamp;

            // init first label
            if (mLabelPairs.empty())
            {
                mLabelPairs.push_back({ "default", firstFrameTimestamp, 0 });
            }
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
            lines = results.dumpsys_meminfo;
            if (lines.size() > 1)
            {
                MemoryStat stat;
                for (auto& line : lines)
                {
                    if (line.find("Native Heap") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssNativeHeap = fromString<float>(tokens[2]) / 1024; // KB -> MB
                    }
                    else if (line.find("EGL mtrack") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssEGL = fromString<float>(tokens[2]) / 1024;
                    }
                    else if (line.find("Gfx dev") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssGfx = fromString<float>(tokens[2]) / 1024;
                    }
                    else if (line.find("GL mtrack") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssGL = fromString<float>(tokens[2]) / 1024;
                    }
                    else if (line.find("Unknown") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssUnknown = fromString<float>(tokens[1]) / 1024;
                    }
                    else if (line.find("TOTAL") != string::npos)
                    {
                        auto& tokens = split(line, ' ');
                        stat.pssTotal = fromString<float>(tokens[1]) / 1024;
                        stat.privateDirty = fromString<float>(tokens[2]) / 1024;
                        stat.privateClean = fromString<float>(tokens[3]) / 1024;

                        mMemorySummary.update(stat.pssTotal, mMemoryStats.size());

                        break;
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
                if (!mTemparatureStatSlot.cpu.empty())
                    mCpuTempSummary.update(results.temperature.cpu, mTemperatureStats.size());
                mTemperatureStats.push_back({ millisec_since_epoch, results.temperature });
            }
        }
    }
    return true;
}

int PerfDoctorApp::getPid(const string& pacakgeName)
{
    char cmd[256];
    sprintf(cmd, "shell pidof %s", pacakgeName.c_str());
    auto lines = executeAdb(cmd);
    if (lines.empty() || lines[0].empty()) // not running
        return 0;

    return fromString<int>(lines[0]);
}

bool PerfDoctorApp::startApp_ios(const string& pacakgeName)
{
    char cmd[256];
    //if (pid = getPid(pacakgeName)) // already running
        //return true;

    sprintf(cmd, "launch %s", pacakgeName.c_str());
    executeIdb(cmd);

    return true;
}

bool PerfDoctorApp::startApp(const string& pacakgeName)
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

bool PerfDoctorApp::stopApp_ios(const string& pacakgeName)
{
    char cmd[256];
    //if (pid = getPid(pacakgeName)) // already running
        //return true;

    sprintf(cmd, "kill %s", pacakgeName.c_str());
    executeIdb(cmd);

    return true;
}

bool PerfDoctorApp::stopApp(const string& pacakgeName)
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

void PerfDoctorApp::updateMetricsData()
{
    global_min_t = RANGE_START;
    if (!mTimestamps.empty())
    {
        auto finalTimestamp = mTimestamps[mTimestamps.size() - 1];
        global_max_t = max<float>((finalTimestamp - mTimestamps[0]) * 1e-3, (RANGE_START + RANGE_DURATION));
        if (!mLabelPairs.empty())
        {
            mLabelPairs[mLabelPairs.size() - 1].end = finalTimestamp;
        }
    }
    else
    {
        // TODO: a bug here?
        global_max_t = max<float>((mCpuStats[mCpuStats.size() - 1].first - firstCpuStatTimestamp) * 1e-3, (RANGE_START + RANGE_DURATION));
    }

    {
        auto& metrics = storage.metric_storage["frame_time"];
        metrics.name = "frame_time";
        metrics.min_x = -1;
        metrics.max_x = mFrameTimeSummary.Max + 10;
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
        auto& metrics = storage.metric_storage["core_freq"];
        metrics.name = "core_freq";
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

void PerfDoctorApp::getUnrealLog(bool openLogFile)
{
    char str[256];
    sprintf(str, "pull /sdcard/UE4Game/%s/%s/Saved/Logs/%s.log", APP_FOLDER.c_str(), APP_FOLDER.c_str(), APP_FOLDER.c_str());
    executeAdb(str);

    if (openLogFile)
    {
        launchWebBrowser(Url(APP_FOLDER + ".log", true));
    }
}

void PerfDoctorApp::getMemReport()
{
    auto fn = [&]() {
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
    };
    timeline().add(fn, timeline().getCurrentTime() + 1);
}

void PerfDoctorApp::getDumpTicks()
{
    auto fn = [&]() {
        mTickFunctions.clear();

        getUnrealLog();

        int line_startDumpTicks = 0; // track the latest one
        int line_endDumpTicks = 0; // track the latest one
        std::ifstream ifs(APP_FOLDER + ".log");
        if (ifs.is_open())
        {
            std::string line;

            // first pass
            int lineNumber = 0;
            while (std::getline(ifs, line))
            {
                if (line.find("Tick Functions (All)") != string::npos)
                {
                    line_startDumpTicks = lineNumber;
                }
                if (line.find("Total registered tick") != string::npos)
                {
                    line_endDumpTicks = lineNumber;
                }

                lineNumber++;
            }
            CI_ASSERT(line_endDumpTicks > line_startDumpTicks);

            // second pass
            lineNumber = 0;
            ifs.clear();                 // clear fail and eof bits
            ifs.seekg(0, std::ios::beg); // back to the start!
            while (std::getline(ifs, line))
            {
                if (lineNumber > line_startDumpTicks && lineNumber < line_endDumpTicks)
                {
                    line = line.substr(30);
                    auto tokens = split(line, ",");
                    if (tokens.size() != 4) continue;
                    TickFunction tick = { tokens[0], tokens[1].substr(1), tokens[2].substr(23)};
                    tokens = split(tick.actor, ' ');
                    if (tokens.size() == 2)
                    {
                        // has object field
                        tick.type = tokens[0];
                        tick.actor = tokens[1];
                        tokens = split(tick.actor, ':');
                        if (tokens.size() >= 2)
                        {
                            // separate level and actor
                            tick.level = tokens[0];
                            auto tk = tokens[1];
                            if (tk.find("PersistentLevel") != string::npos)
                                tk = tk.substr(sizeof("PersistentLevel"));

                            tokens = split(tk, "[].");
                            if (tokens.size() > 2)
                                tick.component = tokens[1];
                            if (tokens.size() > 1)
                                tick.actor = tokens[0];
                        }

                    }
                    mTickFunctions.push_back(tick);
                }
                lineNumber++;
            }

            if (!mTickFunctions.empty())
            {
                auto ts = getTimestampForFilename();
                string csv_name = APP_FOLDER + "-ticks-" + ts + ".csv";
                ofstream ofs(getAppPath() / csv_name);
                if (ofs.is_open())
                {
                    ofs << "Actor,Component,Type,Level,Status,TickGroup" << endl;
                    for (const auto& item : mTickFunctions)
                    {
                        ofs << item.actor << ","
                            << item.component << ","
                            << item.type << ","
                            << item.level << "," 
                            << item.status << "," 
                            << item.tick_group << endl;
                    }
                }
                ofs.close();
                launchWebBrowser(Url(csv_name, true));
            }
        }

        //char str[256];
        //sprintf(str, "shell ls -l /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports", APP_FOLDER.c_str(), APP_FOLDER.c_str());
        //auto lines = executeAdb(str);
        //if (lines.size() > 1)
        //{
        //    auto folder = lines[lines.size() - 1];;
        //    auto tokens = split(folder, ' ');
        //    folder = tokens[tokens.size() - 1];

        //    sprintf(str, "shell ls -l /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports/%s", APP_FOLDER.c_str(), APP_FOLDER.c_str(), folder.c_str());
        //    lines = executeAdb(str);
        //    if (lines.size() > 1)
        //    {
        //        auto lastLine = lines[lines.size() - 1];
        //        tokens = split(lastLine, ' ');
        //        auto lastFile = tokens[tokens.size() - 1];
        //        sprintf(str, "pull /sdcard/UE4Game/%s/%s/Saved/Profiling/MemReports/%s/%s", APP_FOLDER.c_str(), APP_FOLDER.c_str(), folder.c_str(), lastFile.c_str());
        //        executeAdb(str);
        //        launchWebBrowser(Url(lastFile, true));
        //    }
        //}
    };
    timeline().add(fn, timeline().getCurrentTime() + 1);
}

extern void createConfigImgui(WindowRef window = getWindow(), bool autoDraw = true, bool autoRender = true);

void PerfDoctorApp::setup()
{
    SetUnhandledExceptionFilter(unhandled_handler);
    am::addAssetDirectory(getAppPath());
    mAutoStart = AUTO_START;
    ::SetCurrentDirectoryA(getAppPath().string().c_str());


    mMixDevice.setup();

    std::ifstream ifs_perfetto(getAppPath() / "perfetto-template.txt");
    if (ifs_perfetto.is_open())
    {
        std::string line;
        while (std::getline(ifs_perfetto, line))
        {
            perfettoCmdTemplate += line;
            perfettoCmdTemplate += '\n';
        }
    }

    std::ifstream ifs_unreal(getAppPath() / "unreal-cmd.txt");
    if (ifs_unreal.is_open())
    {
        std::string line;
        while (std::getline(ifs_unreal, line))
        {
            mUnrealCmds.push_back(line);
        }
    }

    log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
    createConfigImgui(getWindow(), false);

    implotCtx = ImPlot::CreateContext();
    ImPlot::PushColormap(ImPlotColormap_Cool);

    ImPlot::GetStyle().AntiAliasedLines = true;
    ImPlot::GetStyle().Marker = ImPlotMarker_Circle;
    ImPlot::GetStyle().MarkerSize = 2;
    ImPlot::GetStyle().Colormap = COLOR_MAP;

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
                    launchWebBrowser(Url("https://ui.perfetto.dev/#!/", true));
                    goto_folder(getAppPath(), getAppPath() / (cmds[1] + ".perfetto"));
                }
                else if (asyncCmd.find("screenshot") != string::npos && asyncCmd.find("hide_screenshot") == string::npos)
                {
                    auto cmds = split(asyncCmd, " ");
                    launchWebBrowser(Url(cmds[1] + ".png", true));
                }
            }

            if (mPackageName.empty() || !mIsProfiling || getElapsedSeconds() - lastTimestamp < REFRESH_SECONDS)
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
            else if (SUPPORT_NON_GAME && storage.metric_storage["fps"].visible)
            {
                sprintf(cmd, "shell dumpsys gfxinfo %s framestats", mPackageName.c_str());
                results.dumpsys_gfxinfo = executeAdb(cmd);
            }
            results.EPOCHREALTIME = executeAdb("shell echo $EPOCHREALTIME");
            if (storage.metric_storage["cpu_usage"].visible || storage.metric_storage["core_usage"].visible)
            {
                results.proc_stat = executeAdb("shell cat /proc/stat");
            }

            if (storage.metric_storage["memory_usage"].visible)
            {
                sprintf(cmd, "shell dumpsys meminfo %s", mPackageName.c_str());
                results.dumpsys_meminfo = executeAdb(cmd);
            }

            if (storage.metric_storage["core_freq"].visible)
            {
                results.scaling_cur_freq = executeAdb("shell cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq");
            }

            if (storage.metric_storage["cpu_usage"].visible)
            {
                sprintf(cmd, "shell cat /proc/%d/stat", pid);
                results.proc_pid_stat = executeAdb(cmd);
                if (!results.proc_pid_stat.empty() && results.proc_pid_stat[0].find("error") != string::npos)
                {
                    results.success = false;
                }
            }

            {
                if (!mTemparatureStatSlot.cpu.empty())
                {
                    sprintf(cmd, "shell cat %s", mTemparatureStatSlot.cpu.c_str());
                    auto lines = executeAdb(cmd);
                    if (!lines.empty()) results.temperature.cpu = stoi(lines[0]) * 1e-3;
                }
                if (!mTemparatureStatSlot.gpu.empty())
                {
                    sprintf(cmd, "shell cat %s", mTemparatureStatSlot.gpu.c_str());
                    auto lines = executeAdb(cmd);
                    if (!lines.empty()) results.temperature.gpu = stoi(lines[0]) * 1e-3;
                }
                if (!mTemparatureStatSlot.battery.empty())
                {
                    sprintf(cmd, "shell cat %s", mTemparatureStatSlot.battery.c_str());
                    auto lines = executeAdb(cmd);
                    if (!lines.empty()) results.temperature.battery = stoi(lines[0]) * 1e-3;
                }
            }

            mAdbResults.pushFront(results);
        }
    });

    refreshDeviceNames();

    mLastUpdateTime = getElapsedSeconds();

    getWindow()->getSignalKeyDown().connect([&](KeyEvent& event) {
        if (event.getCode() == KeyEvent::KEY_ESCAPE)
        {
            fps_visible = storage.metric_storage["fps"].visible;
            frame_time_visible = storage.metric_storage["frame_time"].visible;
            cpu_usage_visible = storage.metric_storage["cpu_usage"].visible;
            core_usage_visible = storage.metric_storage["core_usage"].visible;
            memory_usage_visible = storage.metric_storage["memory_usage"].visible;
            core_freq_visible = storage.metric_storage["core_freq"].visible;
            temperature_visible = storage.metric_storage["temperature"].visible;

            COLOR_MAP = ImPlot::GetStyle().Colormap;

            quit();
        }
    });

    getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
        if (event.isControlDown() && event.getCode() == KeyEvent::KEY_l) getUnrealLog(true);
        if (event.isControlDown() && event.getCode() == KeyEvent::KEY_s) screenshot();
        if (event.isControlDown() && event.getCode() == KeyEvent::KEY_p) capturePerfetto();
        if (event.isControlDown() && event.getCode() == KeyEvent::KEY_m) captureSimpleperf();
        if (event.isControlDown() && event.getCode() == KeyEvent::KEY_d) executeUnrealCmd("dumpticks");
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
        //ImGui::CaptureKeyboardFromApp(false);

        if (ImGui::Begin("Devices", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            drawLeftSidePanel();
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

CINDER_APP(PerfDoctorApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
