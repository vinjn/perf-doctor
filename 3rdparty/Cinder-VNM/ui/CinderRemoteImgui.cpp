#include <winsock2.h>
#include "CinderRemoteImgui.h"
#include <cinder/app/App.h>
#include <cinder/CinderImGui.h>
#include "../ui/imgui_remote/imgui_remote.h"

extern void    ImGui_ImplOpenGL3_NewFrame();
extern void    ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);

static bool sTriggerNewFrame = false;

void createRemoteImgui(const char* address, int port)
{
    ImGui::RemoteInit(address, port);
    sTriggerNewFrame = true;
}

void updateRemoteImgui(bool ENABLE_REMOTE_GUI)
{
    if (!ENABLE_REMOTE_GUI) return;

    ImGui::RemoteUpdate();
    ImGui::RemoteInput input;
    if (ImGui::RemoteGetInput(input))
    {
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < 256; i++)
            io.KeysDown[i] = input.KeysDown[i];
        io.KeyCtrl = input.KeyCtrl;
        io.KeyShift = input.KeyShift;
        io.MousePos = input.MousePos;
        io.MouseDown[0] = (input.MouseButtons & 1);
        io.MouseDown[1] = (input.MouseButtons & 2) != 0;
        io.MouseWheel += input.MouseWheelDelta;
        // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
        io.KeyMap[ImGuiKey_Tab] = ImGuiKey_Tab;
        io.KeyMap[ImGuiKey_LeftArrow] = ImGuiKey_LeftArrow;
        io.KeyMap[ImGuiKey_RightArrow] = ImGuiKey_RightArrow;
        io.KeyMap[ImGuiKey_UpArrow] = ImGuiKey_UpArrow;
        io.KeyMap[ImGuiKey_DownArrow] = ImGuiKey_DownArrow;
        io.KeyMap[ImGuiKey_Home] = ImGuiKey_Home;
        io.KeyMap[ImGuiKey_End] = ImGuiKey_End;
        io.KeyMap[ImGuiKey_Delete] = ImGuiKey_Delete;
        io.KeyMap[ImGuiKey_Backspace] = ImGuiKey_Backspace;
        io.KeyMap[ImGuiKey_Enter] = 13;
        io.KeyMap[ImGuiKey_Escape] = 27;
        io.KeyMap[ImGuiKey_A] = 'a';
        io.KeyMap[ImGuiKey_C] = 'c';
        io.KeyMap[ImGuiKey_V] = 'v';
        io.KeyMap[ImGuiKey_X] = 'x';
        io.KeyMap[ImGuiKey_Y] = 'y';
        io.KeyMap[ImGuiKey_Z] = 'z';
    }
}



void ImGui_ImplCinder_NewFrameGuard(const ci::app::WindowRef& window) {
    if (!sTriggerNewFrame)
        return;

    ImGui_ImplOpenGL3_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt()); // Font atlas needs to be built, call renderer _NewFrame() function e.g. ImGui_ImplOpenGL3_NewFrame() 

    // Setup display size
    io.DisplaySize = ci::app::toPixels(window->getSize());

    // Setup time step
    static double g_Time = 0.0f;
    double current_time = ci::app::getElapsedSeconds();
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f / 60.0f);
    g_Time = current_time;

    ImGui::NewFrame();

    sTriggerNewFrame = false;
}

void ImGui_ImplCinder_PostDraw(bool ENABLE_REMOTE_GUI, bool ENABLE_LOCAL_GUI)
{
    ImGui::Render();
    auto draw_data = ImGui::GetDrawData();
    if (ENABLE_REMOTE_GUI)
        ImGui::RemoteDraw(draw_data->CmdLists, draw_data->CmdListsCount);
    if (ENABLE_LOCAL_GUI)
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    sTriggerNewFrame = true;
}
