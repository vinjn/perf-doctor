#pragma once

#include "cinder/gl/Texture.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/audio/Voice.h"

#include <string>
#include <vector>

#define TEXTURE_3D_ENABLED 0

namespace am // am -> asset manager
{
    void addAssetDirectory(const ci::fs::path& dirPath);

    ci::BufferRef& buffer(const std::string& relativeName);

    ci::SurfaceRef& surface(const std::string& relativeName, bool forceAlpha = false);
    ci::Surface16uRef& surface16u(const std::string& relativeName, bool forceAlpha = false);
    ci::Surface32fRef& surface32f(const std::string& relativeName, bool forceAlpha = false);
    ci::ChannelRef& channel(const std::string& relativeName);
    ci::Channel16uRef& channel16u(const std::string& relativeName);

    // Supports jpg, png, bmp, tga, dds, ktx, hdr etc.
    // Special support: "checkerboard"
#if ! defined( CINDER_GL_ES )
    //ci::gl::Texture1dRef& texture1d(const std::string& relativeName, const ci::gl::Texture1d::Format& format = ci::gl::Texture1d::Format(), bool isAsync = false);
#endif
    ci::gl::Texture2dRef& texture2d(const std::string& relativeName,
                                    const ci::gl::Texture2d::Format& format = ci::gl::Texture2d::Format()
                                        .mipmap().minFilter(GL_LINEAR_MIPMAP_LINEAR).magFilter(GL_LINEAR).wrap(GL_REPEAT),
                                    bool isAsync = false);
#if TEXTURE_3D_ENABLED
    ci::gl::Texture3dRef& texture3d(const std::string& relativeName, const ci::gl::Texture3d::Format& format = ci::gl::Texture3d::Format()
                                    .mipmap().minFilter(GL_LINEAR_MIPMAP_LINEAR).magFilter(GL_LINEAR),
                                    bool isAsync = false);
#endif
    ci::gl::TextureCubeMapRef& textureCubeMap(const std::string& relativeName,
                                              const ci::gl::TextureCubeMap::Format& format = ci::gl::TextureCubeMap::Format()
                                              .mipmap().minFilter(GL_LINEAR_MIPMAP_LINEAR).magFilter(GL_LINEAR),
                                              bool isAsync = false);

    // Supports obj, msh.
    // Special support: "Rect", "Icosahedron", "Icosphere", "Teapot", "Circle", "Ring", "Sphere", "Capsule" etc
    ci::TriMeshRef& triMesh(const std::string& objFileName, const std::string& mtlFileName = "");

    ci::gl::VboMeshRef& vboMesh(const std::string& objFileName, const std::string& mtlFileName = "");

    // Special support built-in shaders: "passthrough", "lambert texture", "color", "texture", "lambert", "color texture" etc
    ci::gl::GlslProgRef& glslProg(const std::string& vsFileName, const std::string& fsFileName = "", ci::gl::GlslProg::Format format = {});

    std::string& str(const std::string& relativeName);

    std::vector<std::string>& longPaths(const std::string& relativeFolderName);
    std::vector<std::string>& shortPaths(const std::string& relativeFolderName);

    ci::audio::VoiceRef& voice(const std::string& relativeName);
}
