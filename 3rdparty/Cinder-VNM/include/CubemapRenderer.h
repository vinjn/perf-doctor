#pragma once

#include <cinder/gl/gl.h>
#include <cinder/app/App.h>

struct CubeMapRenderer
{
    ci::gl::FboCubeMapRef fbo;
    ci::CameraPersp camera;

    void setup(int width = 1024, int height = 1024)
    {
        fbo = ci::gl::FboCubeMap::create(width, height);
        camera = ci::CameraPersp(fbo->getWidth(), fbo->getHeight(), 90.0f, 1, 1000);
    }

    ci::signals::Signal<void()> signalDraw;

    void draw()
    {
        ci::gl::ScopedDebugGroup debug("CubeMap::draw()");
        ci::gl::ScopedMatrices matrices;
        ci::gl::ScopedViewport viewport(fbo->getSize());
        // we need to save the current FBO because we'll be calling bindFramebufferFace() below
        ci::gl::context()->pushFramebuffer();
        for (uint8_t dir = 0; dir < 6; ++dir)
        {
            ci::gl::setProjectionMatrix(camera.getProjectionMatrix());
            ci::gl::setViewMatrix(fbo->calcViewMatrix(GL_TEXTURE_CUBE_MAP_POSITIVE_X + dir, ci::vec3(0)));
            fbo->bindFramebufferFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X + dir);

            signalDraw.emit();
        }
        // restore the FBO before we bound the various faces of the CubeMapFbo
        ci::gl::context()->popFramebuffer();
    }

    void bindTexture(int slot = 0)
    {
        fbo->bindTexture(slot);
    }
    void drawDebugView(ci::Rectf screenRect = ci::Rectf(0, 0, 600, 300), int debugType = 0)
    {
        ci::gl::setMatricesWindow(ci::app::getWindowSize());
        ci::gl::ScopedDepth d(false);
        if (debugType == 0)
            ci::gl::drawEquirectangular(fbo->getTextureCubeMap(), screenRect);
        else if (debugType == 0)
            ci::gl::drawHorizontalCross(fbo->getTextureCubeMap(), screenRect);
        else 
            ci::gl::drawVerticalCross(fbo->getTextureCubeMap(), screenRect);
    }
};