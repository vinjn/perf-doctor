#pragma once

#include "imgui/imgui.h"
#include "cinder/Log.h"

// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
namespace ImGui
{
    struct DearLogger : ci::log::Logger
    {
        ImGuiTextBuffer     Buf;
        ImGuiTextFilter     Filter;
        ImVector<int>       LineOffsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
        bool                AutoScroll;
        bool                ScrollToBottom;

        DearLogger();

        void    Clear();

        void write(const ci::log::Metadata& meta, const std::string& text) override;


        void    AddLog(const char* fmt, ...) IM_FMTARGS(2);

        void    Draw(const char* title, bool* p_open = NULL);
    };
}
