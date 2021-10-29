#pragma once

#include "cinder/app/App.h"
#include "cinder/Capture.h"
#include "cinder/Log.h"
#include "cinder/ip/Flip.h"
#include "cinder/gl/Texture.h"

using namespace ci;
using namespace ci::app;

struct CaptureHelper
{
    Surface         surface;
    gl::TextureRef  texture;
    ivec2           size;
    bool            flip;
    std::string     deviceName;
    bool            isBackCamera;
    
    bool isReady()
    {
        return texture != nullptr;
    }
    
    bool checkNewFrame()
    {
        bool ret = hasNewFrame;
        if (hasNewFrame) hasNewFrame = false;
        return ret;
    }
    
    void setup(int32_t width = 640, int32_t height = 480, const Capture::DeviceRef device = Capture::DeviceRef())
    {
        try
        {
            hasNewFrame = false;

            capture = Capture::create(width, height, device);
            capture->start();
            flip = false;
            deviceName = capture->getDevice()->getName();
            isBackCamera = (deviceName.find("Back") != std::string::npos);
            
            auto updateFn = [this]
            {
                if (capture && capture->checkNewFrame())
                {
                    surface = *capture->getSurface();
                    if (flip) ip::flipHorizontal(&surface);

                    size = surface.getSize();
                    
                    if (!texture)
                    {
                        // Capture images come back as top-down, and it's more efficient to keep them that way
                        gl::Texture::Format format;
//                        format.loadTopDown();
                        texture = gl::Texture::create( surface, format);
                    }
                    else
                    {
                        texture->update( surface );
                    }
                    
                    hasNewFrame = true;
                }
            };
            App::get()->getSignalUpdate().connect(updateFn);
            
        }
        catch ( ci::Exception &exc )
        {
            CI_LOG_EXCEPTION( "Failed to init capture ", exc );
        }
    }
    
    static void printDevices()
    {
        for ( const auto &device : Capture::getDevices() )
        {
            console() << "Device: " << device->getName() << " "
#if defined( CINDER_COCOA_TOUCH )
            << ( device->isFrontFacing() ? "Front" : "Rear" ) << "-facing"
#endif
            << std::endl;
        }
    }
    
private:
    
    CaptureRef  capture;
    bool        hasNewFrame;

};
