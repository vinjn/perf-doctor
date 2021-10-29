#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct _TBOX_PREFIX_App : public App
{
    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");
        createConfigImgui();

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getWindow()->getSignalResize().connect([&]{
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getSignalUpdate().connect([&] {
        });

        getSignalCleanup().connect([&] { writeConfig(); });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
        });
    }
};

CINDER_APP( _TBOX_PREFIX_App, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
