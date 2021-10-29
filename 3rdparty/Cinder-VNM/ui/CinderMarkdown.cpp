#include <cinder/Utilities.h>
#include "imgui/ImGui.h"                // https://github.com/ocornut/imgui
#include "imgui_markdown/imgui_markdown.h"       // https://github.com/juliettef/imgui_markdown
#include "IconFontCppHeaders/IconsFontAwesome5.h"    // https://github.com/juliettef/IconFontCppHeaders


using namespace ci;

/////

void LinkCallback(ImGui::MarkdownLinkCallbackData data_);
inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data_);

// You can make your own Markdown function with your prefered string container and markdown config.
static ImGui::MarkdownConfig mdConfig{ LinkCallback, ImageCallback, ICON_FA_LINK, { { NULL, true }, { NULL, true }, { NULL, false } } };

void LinkCallback(ImGui::MarkdownLinkCallbackData data_)
{
    std::string url(data_.link, data_.linkLength);
    launchWebBrowser(Url(url));
}

inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data_)
{
    // In your application you would load an image based on data_ input. Here we just use the imgui font texture.
    ImTextureID image = ImGui::GetIO().Fonts->TexID;
    ImGui::MarkdownImageData imageData{ true, false, image, ImVec2(40.0f, 20.0f) };
    return imageData;
}

void LoadFonts(float fontSize_ = 12.0f)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    // Base font
    io.Fonts->AddFontFromFileTTF("myfont.ttf", fontSize_);
    // Bold headings H2 and H3
    mdConfig.headingFormats[1].font = io.Fonts->AddFontFromFileTTF("myfont-bold.ttf", fontSize_);
    mdConfig.headingFormats[2].font = mdConfig.headingFormats[1].font;
    // bold heading H1
    float fontSizeH1 = fontSize_ * 1.1f;
    mdConfig.headingFormats[0].font = io.Fonts->AddFontFromFileTTF("myfont-bold.ttf", fontSizeH1);
}

void Markdown(const std::string& markdown_)
{
    // fonts for, respectively, headings H1, H2, H3 and beyond
    ImGui::Markdown(markdown_.c_str(), markdown_.length(), mdConfig);
}

void MarkdownExample()
{
    const std::string markdownText = u8R"(
# H1 Header: Text and Links
You can add [links like this one to enkisoftware](https://www.enkisoftware.com/) and lines will wrap well.
## H2 Header: indented text.
  This text has an indent (two leading spaces).
    This one has two.
### H3 Header: Lists
  * Unordered lists
    * Lists can be indented with two extra spaces.
  * Lists can have [links like this one to Avoyd](https://www.avoyd.com/)
)";
    Markdown(markdownText);
}
