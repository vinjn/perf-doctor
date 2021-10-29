#pragma once

#include "MiniConfig.h"
#include <Cinder/CinderImGui.h>
#include <cinder/Utilities.h>
#include <cinder/app/App.h>

using namespace ci;
using namespace ci::app;
using namespace std;

namespace vnm
{
    void drawFrameTime()
    {
        ImGuiIO& io = ImGui::GetIO();
        static float values[300] = {};
        static int values_offset = 0;
        values[values_offset] = io.DeltaTime * 1000;
        values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
        ImGui::PlotLines("frame time (ms)", values, IM_ARRAYSIZE(values), values_offset, NULL, 0.0f, 30, ImVec2(0, 80));
    }

    bool addImguiParam(const char* label, int& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputInt(label, &v, 1, 100, flags);
    }

    bool addImguiParam(const char* label, float& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputFloat(label, &v, 0, 0, "%.3f", flags);
    }

    bool addImguiParam(const char* label, double& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputDouble(label, &v, 0, 0, "%.6f", flags);
    }

    bool addImguiParam(const char* label, float& v, float min, float max)
    {
        v = std::max(std::min(v, max), min);
        return ImGui::DragFloat(label, &v, 1.0f, min, max);
    }

    bool addImguiParam(const char* label, double& v, double min, double max)
    {
        v = std::max(std::min(v, max), min);
        return ImGui::DragScalar(label, ImGuiDataType_Double, &v, 1.0f, &min, &max, "%.8f");
    }

    bool addImguiParam(const char* label, int& v, int min, int max)
    {
        v = std::max(std::min(v, max), min);
        return ImGui::DragInt(label, &v, 1.0f, min, max);
    }

    bool addImguiParamSlider(const char* label, float& v, float min, float max)
    {
        return ImGui::SliderFloat(label, &v, min, max);
    }

    bool addImguiParamSlider(const char* label, double& v, double min, double max)
    {
        return ImGui::SliderScalar(label, ImGuiDataType_Double, &v, &min, &max, "%.8f");
    }

    bool addImguiParamSlider(const char* label, int& v, int min, int max)
    {
        return ImGui::SliderInt(label, &v, min, max);
    }

    bool addImguiParam(const char* label, bool& v)
    {
        return ImGui::Checkbox(label, &v);
    }

    bool addImguiParamRadio(const char* label, int& v, int v_button)
    {
        return ImGui::RadioButton(label, &v, v_button);
    }

    bool addImguiParam(const char* label, string& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputText(label, &v, flags);
    }

    bool addImguiParam(const char* label, quat& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputFloat4(label, &v.x, -1, flags);
    }

    bool addImguiParam(const char* label, vec2& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputFloat2(label, &v.x, -1, flags);
    }

    bool addImguiParam(const char* label, ivec2& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputInt2(label, &v.x, flags);
    }

    bool addImguiParam(const char* label, vec3& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputFloat3(label, &v.x, -1, flags);
    }

    bool addImguiParam(const char* label, vec4& v)
    {
        ImGuiInputTextFlags flags = 0;
        if (label[0] == '_') flags = ImGuiInputTextFlags_ReadOnly;
        return ImGui::InputFloat4(label, &v.x, -1, flags);
    }

    bool addImguiParam(const char* label, Color& v)
    {
        return ImGui::ColorEdit3(label, &v.r);
    }

    bool addImguiParam(const char* label, ColorA& v)
    {
        return ImGui::ColorEdit4(label, &v.r, true);
    }

    void drawMinicofigImgui(bool createNewWindow = false)
    {
        if (createNewWindow)
            ImGui::Begin(CONFIG_XML, NULL, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::Button("Save"))
        {
            writeConfig();
        }

        ImGui::SameLine();
        if (ImGui::Button("Screenshot"))
        {
            takeScreenShot();
        }

        ImGui::SameLine();
        if (ImGui::Button("Quit"))
        {
            App::get()->quit();
        }
        
        auto profilerHtml = getAssetPath("webui_remotery/index.html");
        if (fs::exists(profilerHtml))
        {
            ImGui::SameLine();
            if (ImGui::Button("Profiler"))
            {
                launchWebBrowser(Url(profilerHtml.string(), true));
            }
        }

#if !defined(NDEBUG) && !defined(IMGUI_DISABLE_DEMO_WINDOWS)
        static bool isDemoWindowOpened = false;
        if (ImGui::Button("ShowDemoWindow"))
            isDemoWindowOpened = !isDemoWindowOpened;
        if (isDemoWindowOpened)
            ImGui::ShowDemoWindow(&isDemoWindowOpened);
#endif  
        
#define GROUP_DEF(grp)                      } if (ImGui::CollapsingHeader(#grp, #grp[0] == '_' ? ImGuiTreeNodeFlags_CollapsingHeader : ImGuiTreeNodeFlags_DefaultOpen)) {
#define ITEM_DEF(type, var, default)        addImguiParam(#var, var);
#define ITEM_DEF_DIGIT(type, var, default, Min, Max)   addImguiParam(#var, var, Min, Max);
#define ITEM_DEF_MINMAX(type, var, default, Min, Max)  addImguiParamSlider(#var, var, Min, Max);
#define ITEM_DEF_RADIO(label, var, button)             addImguiParamRadio(#label, var, button);
#define GROUP_DEF_RADIO(var, default)
        if (true)
        {
#include "item.def"
        }
#undef GROUP_DEF_RADIO
#undef ITEM_DEF_RADIO
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF_DIGIT
#undef ITEM_DEF
#undef GROUP_DEF

        if (createNewWindow)
            ImGui::End();
    }
}

//! autoDraw: Specify whether the block should call vnm::drawMinicofigImgui automatically. Default to true.
//! autoRender: Specify whether the block should call ImGui::NewFrame and ImGui::Render automatically. Default to true.
void createConfigImgui(WindowRef window = getWindow(), bool autoDraw = true, bool autoRender = true)
{
    auto option = ImGui::Options()
        .window(window)
        .autoRender(autoRender)
        .iniPath(App::get()->getAppPath() / "imgui.ini");
    ImGui::Initialize(option);
    if (autoDraw)
        App::get()->getSignalUpdate().connect([] {vnm::drawMinicofigImgui(true); });
}
