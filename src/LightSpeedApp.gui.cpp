#include "LightSpeedApp.h"
#include "MiniConfigImgui.h"


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

// Plots axis-aligned, filled rectangles. Every two consecutive points defines opposite corners of a single rectangle.
static ImPlotPoint label_getter(void* data, int idx)
{
    auto* self = (LabelPair*)data;
    int span_idx = idx / 2;
    int tag = idx % 2;
    if (tag == 0)
    {
        float start_t = (self[span_idx].start - firstFrameTimestamp) * 1e-3;
        return ImPlotPoint(start_t, 0);
    }
    else
    {
        float end_t = (self[span_idx].end - firstFrameTimestamp) * 1e-3;
        return ImPlotPoint(end_t, 10);
    }
}

void PerfDoctorApp::drawLeftSidePanel()
{
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("DeviceTab", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Devices"))
        {
            drawDeviceTab();
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
            ImGui::SameLine();
            if (ImGui::Button("Get log"))
            {
                getUnrealLog();
                launchWebBrowser(Url(APP_FOLDER + ".log", true));
            }

            ImGui::InputText(" ", &UE_CMD);
            ImGui::SameLine();
            if (ImGui::Button("Apply"))
            {
                executeUnrealCmd(UE_CMD);
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
                        executeUnrealCmd(UE_CMD);
                    }
                }
            }

            ImGui::EndTabItem();
        }
    }
    ImGui::EndTabBar();
}

void PerfDoctorApp::drawDeviceTab()
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

            {
                if (ImGui::Button("Trim Memory")) trimMemory("RUNNING_MODERATE");
                ImGui::SameLine();
                if (ImGui::Button("Low")) trimMemory("RUNNING_LOW");
                ImGui::SameLine();
                if (ImGui::Button("Critical")) trimMemory("RUNNING_CRITICAL");
            }
            
            ImGui::Unindent();

            if (ImGui::Button("Screenshot"))
            {
                screenshot();
            }
            ImGui::SameLine();

            if (ImGui::Button("Capture Perfetto"))
            {
                capturePerfetto();
            }

            if (mIsProfiling)
            {
                if (ImGui::Button("Stop Profiling"))
                {
                    stopProfiler();
                }

                ImGui::SameLine();

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
            if (!mDeviceStat.gpu_name.empty())
                ImGui::Text("%s", mDeviceStat.gpu_name.c_str());
            if (mDeviceStat.width > 0 && mDeviceStat.height > 0)
                ImGui::Text("%d x %d", mDeviceStat.width, mDeviceStat.height);
            if (!mDeviceStat.gfx_api_version.empty())
                ImGui::Text(mDeviceStat.gfx_api_version.c_str());
            for (const auto& config : mCpuConfigs)
            {
                if (config.cpuinfo_min_freq > 0)
                {
                    ImGui::Text("cpu_%d: %s %.2f~%.2f GHz",
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

void PerfDoctorApp::drawPerfPanel()
{
#if 0
    for (const auto& kv : storage.span_storage)
    {
        auto& series = kv.second;

        ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
        ImPlot::SetNextPlotLimitsX(global_min_t, global_max_t, ImGuiCond_Always);
        ImPlot::SetNextPlotTicksY(0, 10, 2);

        if (ImPlot::BeginPlot(series_name.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT), ImPlotFlags_NoChild, ImPlotAxisFlags_None))
        {
            //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
            ImPlot::PlotRects(series_name.c_str(), SpanSeries::getter, (void*)&series, series.span_array.size() * 2);
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
#endif

    bool s_drawLabel = true;
    for (const auto& kv : storage.metric_storage)
    {
        auto& series = kv.second;
        auto& series_name = kv.first;
        if (!series.visible) continue;

        if (s_drawLabel)
        {
            s_drawLabel = false;
            float height = 1;
            ImPlot::SetNextAxisLimits(ImAxis_X1, global_min_t, global_max_t, ImGuiCond_Always);
            ImPlot::SetNextAxisLimits(ImAxis_Y1, 0, height, ImGuiCond_Always);
            auto color = ImVec4(0.3f, 0.3f, 0.3f, 0.5f);
            ImPlot::PushStyleColor(ImPlotCol_Fill, color);

            if (ImPlot::BeginPlot("label", NULL, NULL, ImVec2(-1, 60),
                ImPlotFlags_NoLegend | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus, ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks, ImPlotAxisFlags_NoDecorations))
            {
                // TODO:
                //ImPlot::PlotRects("label", label_getter, (void*)mLabelPairs.data(), mLabelPairs.size() * 2);
                for (const auto& pair : mLabelPairs)
                {
                    float start = (pair.start - firstFrameTimestamp) * 1e-3;
                    float end = (pair.end - firstFrameTimestamp) * 1e-3;

                    ImPlot::PlotText(pair.name.c_str(), (start + end) * 0.5, height / 2, false);
                }
                ImPlot::EndPlot();
            }

            ImPlot::PopStyleColor();
        }

        //ImPlot::SetNextPlotTicksX(global_min_t, global_max_t, PANEL_TICK_T);
        ImPlot::SetNextAxisLimits(ImAxis_X1, global_min_t, global_max_t, ImGuiCond_Always);
        //ImPlot::SetNextPlotTicksY(series.min_x, series.max_x, PANEL_TICK_X);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, series.min_x, series.max_x, ImGuiCond_Always);

        string title = series_name;
        char text[256];

        if (series_name == "fps" && !mFpsArray.empty())
        {
            sprintf(text, "fps [%.1f, %.1f] avg: %.1f", mFpsSummary.Min, mFpsSummary.Max, mFpsSummary.Avg);
            title = text;
        }
        if (series_name == "memory_usage" && !mMemoryStats.empty())
        {
            sprintf(text, "memory_usage [%.0f, %.0f] avg: %.0f", mMemorySummary.Min, mMemorySummary.Max, mMemorySummary.Avg);
            title = text;
        }
        if (series_name == "temperature" && !mTemperatureStats.empty())
        {
            sprintf(text, "temperature [%.0f, %.0f] avg: %.0f", mCpuTempSummary.Min, mCpuTempSummary.Max, mCpuTempSummary.Avg);
            title = text;
        }
        if (series_name == "cpu_usage" && !mAppCpuStats.empty())
        {
            sprintf(text, "cpu_usage [%.0f, %.0f] avg: %.1f", mAppCpuSummary.Min, mAppCpuSummary.Max, mAppCpuSummary.Avg);
            //title = text;
            // TODO: fix bug
        }
        if (ImPlot::BeginPlot(title.c_str(), NULL, NULL, ImVec2(-1, PANEL_HEIGHT),
            ImPlotFlags_NoChild | ImPlotFlags_NoMenus, ImPlotAxisFlags_NoDecorations))
        {
            ImPlot::SetupLegend(ImPlotLocation_North | ImPlotLocation_West);

            //ImPlot::PushStyleColor(ImPlotCol_Line, items[i].Col);
            if (series_name == "frame_time")
            {
                ImPlot::PlotLineG(series_name.c_str(), frameTime_getter, (void*)&mFrameTimes, mFrameTimes.size());
            }
            else if (series_name == "fps")
            {
                ImPlot::PlotLineG(series_name.c_str(), fps_getter, (void*)&mFpsArray, mFpsArray.size());
            }
            else if (series_name == "cpu_usage")
            {
                ImPlot::PlotLineG("sys", cpuUsage_getter, (void*)&mCpuStats, mCpuStats.size() - 1);
                ImPlot::PlotLineG("app", app_cpuUsage_getter, (void*)&mAppCpuStats, mAppCpuStats.size() - 1);
            }
            else if (series_name == "core_usage")
            {
                char label[] = "cpu_0";
                for (int i = 0; i < mCpuConfigs.size(); i++)
                {
                    label[4] = '0' + i;
                    ImPlot::PlotLineG(label, cpuUsage_getter, (void*)&mChildCpuStats[i], mChildCpuStats[i].size() - 1);
                }
            }
            else if (series_name == "core_freq")
            {
                char label[] = "cpu_0";
                for (int i = 0; i < mCpuConfigs.size(); i++)
                {
                    label[4] = '0' + i;
                    ImPlot::PlotLineG(label, cpuFreq_getter, (void*)&mChildCpuStats[i], mChildCpuStats[i].size());
                }
            }
            else if (series_name == "memory_usage")
            {
                ImPlot::PlotLineG("total", memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                ImPlot::PlotLineG("native_heap", nativeheap_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                ImPlot::PlotLineG("graphics", gl_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                ImPlot::PlotLineG("unknown", unknown_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                //ImPlot::PlotLineG("private_clean", privateClean_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
                //ImPlot::PlotLineG("private_dirty", privateDirty_memoryUsage_getter, (void*)&mMemoryStats, mMemoryStats.size());
            }
            else if (series_name == "temperature")
            {
                if (!mTemparatureStatSlot.cpu.empty())
                    ImPlot::PlotLineG("cpu", temp_cpu_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
                if (!mTemparatureStatSlot.gpu.empty())
                    ImPlot::PlotLineG("gpu", temp_gpu_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
                if (!mTemparatureStatSlot.battery.empty())
                    ImPlot::PlotLineG("battery", temp_battery_getter, (void*)&mTemperatureStats, mTemperatureStats.size());
            }
            else
                ImPlot::PlotLineG(series_name.c_str(), MetricSeries::getter, (void*)&series, series.t_array.size());
            //ImPlot::PopStyleColor();
            ImPlot::EndPlot();
        }
    }
}

void PerfDoctorApp::drawLabel()
{
    if (ImPlot::BeginPlot("label"))
    {
        bool clamp = false;
#if 0
        ImVec4 col = ImVec4(1, 0.5f, 0, 0.25f);
        clamp ? ImPlot::AnnotateClamped(0.25, 0.25, ImVec2(-15, 15), col, "BL") : ImPlot::Annotate(0.25, 0.25, ImVec2(-15, 15), col, "BL");
        clamp ? ImPlot::AnnotateClamped(0.75, 0.25, ImVec2(15, 15), col, "BR") : ImPlot::Annotate(0.75, 0.25, ImVec2(15, 15), col, "BR");
        clamp ? ImPlot::AnnotateClamped(0.75, 0.75, ImVec2(15, -15), col, "TR") : ImPlot::Annotate(0.75, 0.75, ImVec2(15, -15), col, "TR");
        clamp ? ImPlot::AnnotateClamped(0.25, 0.75, ImVec2(-15, -15), col, "TL") : ImPlot::Annotate(0.25, 0.75, ImVec2(-15, -15), col, "TL");
        clamp ? ImPlot::AnnotateClamped(0.5, 0.5, ImVec2(0, 0), col, "Center") : ImPlot::Annotate(0.5, 0.5, ImVec2(0, 0), col, "Center");

        float bx[] = { 1.2f,1.5f,1.8f };
        float by[] = { 0.25f, 0.5f, 0.75f };
        ImPlot::PlotBars("##Bars", bx, by, 3, 0.2);
        for (int i = 0; i < 3; ++i)
            ImPlot::Annotate(bx[i], by[i], ImVec2(0, -5), "B[%d]=%.2f", i, by[i]);
#endif
        ImPlot::EndPlot();
    }
}
