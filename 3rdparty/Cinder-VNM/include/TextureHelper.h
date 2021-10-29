#pragma once

#include "cinder/gl/Texture.h"

using namespace ci;

template <typename T>
void updateTexture(gl::TextureRef &tex, const T &src, const gl::Texture2d::Format& format = gl::Texture2d::Format())
{
    if (src.getWidth() == 0) return;

    if (!tex 
        || tex->getWidth() != src.getWidth()
        || tex->getHeight() != src.getHeight())
    {
        tex = gl::Texture2d::create(src, format);
    }
    else
    {
        tex->update(src);
    }
}

static gl::Texture::Format getTextureFormatUINT16()
{
    return gl::Texture::Format()
        .dataType(GL_UNSIGNED_SHORT)
        .internalFormat(GL_R16UI)
        .minFilter(GL_NEAREST)
        .immutableStorage();
}

static gl::TextureCubeMap::Format getTextureFormatHDRCubeMap()
{
    return gl::TextureCubeMap::Format()
        .mipmap()
        .internalFormat(GL_RGB16F)
        .minFilter(GL_LINEAR_MIPMAP_LINEAR)
        .magFilter(GL_LINEAR)
        .immutableStorage();
}

static gl::Texture::Format getTextureFormatDefault()
{
    return gl::Texture::Format()
        .mipmap()
        .minFilter(GL_LINEAR_MIPMAP_LINEAR)
        .magFilter(GL_LINEAR)
        .wrap(GL_REPEAT);
}
