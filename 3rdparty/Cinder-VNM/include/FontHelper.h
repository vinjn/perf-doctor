#pragma once

#include "cinder/gl/TextureFont.h"

struct FontHelper
{
    static const std::vector<std::string>& getFontNames()
    {
        return ci::Font::getNames();
    }

    // gl::TextureFontRef texFont = createTextureFont();
    // texFont->drawString(str, {30, 30});

    // sans-serif: Helvetica, Arial, Verdana, Trebuchet MS
    // serif: Georgia, Times New Roman, Courier
    static ci::gl::TextureFontRef createTextureFont(const std::string& fontName = "Helvetica", float fontScale = 24)
    {
        auto font = ci::Font(fontName, fontScale);
        return ci::gl::TextureFont::create(font, ci::gl::TextureFont::Format().premultiply());
    }
};
