#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "DepthSensor.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct _TBOX_PREFIX_App : public App
{
    void setup() override
    {
        const auto& args = getCommandLineArgs();
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");

        ds::DeviceType type = (ds::DeviceType)SENSOR_TYPE;
        ds::Option option;
        option.enableColor = true;
        mDevice = ds::Device::create(type, option);
        if (!mDevice->isValid())
        {
            quit();
            return;
        }

        mDevice->signalDepthDirty.connect([&]{
            mDepthChannel = mDevice->depthChannel.clone();
        });

        mCamUi = CameraUi( &mCam, getWindow(), -1 );

        createConfigImgui();
        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
            mCam.setAspectRatio( getWindowAspectRatio() );
        });

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        //mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        //mGlslProg->uniform("uTex0", 0);

        getWindow()->getSignalDraw().connect([&] {
            gl::setMatrices( mCam );
            gl::clear();
        });
    }
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    Channel16u          mDepthChannel;
    gl::GlslProgRef     mGlslProg;
    ds::DeviceRef       mDevice;
};

CINDER_APP( _TBOX_PREFIX_App, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
