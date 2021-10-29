#include "../include/MiniConfig.h"
#include <cinder/Xml.h>
#include <cinder/gl/wrapper.h>
#include <cinder/Utilities.h>
#include <cinder/app/App.h>
#include <cinder/params/Params.h>

#ifndef CINDER_COCOA_TOUCH
#include <cinder/ImageIo.h>
#endif

using namespace ci;
using namespace ci::app;
using namespace std;

#define GROUP_DEF(grp)
#define ITEM_DEF(type, var, default) type var = default;
#define ITEM_DEF_MINMAX(type, var, default, Min, Max) ITEM_DEF(type, var, default);
#include ITEM_DEF_FILE
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF
#undef GROUP_DEF

void readConfig()
{
    fs::path configPath = app::getAppPath() / CONFIG_XML;
    XmlTree tree;
    try
    {
        tree = XmlTree(loadFile(configPath));
    }
    catch (exception& e)
    {
        console() << e.what() << endl;
        console() << "[Warning] Fails to read from " << configPath.string() << endl;
        writeConfig();
        return;
    }
    XmlTree group;

#define GROUP_DEF(grp) try {group = tree.getChild(#grp);} catch (exception& e) {}
#define ITEM_DEF(type, var, default)                        \
    try                                                      \
    {                                                       \
        if (group.getTag().empty())                         \
            var = tree.getChild(#var).getValue<type>();     \
        else                                                \
            var = group.getChild(#var).getValue<type>();    \
    }                                                       \
    catch (exception& e)                                    \
    {                                                       \
        console() << e.what() << endl;                      \
    }

#define ITEM_DEF_MINMAX(type, var, default, Min, Max) ITEM_DEF(type, var, default);
#include "item.def"
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF
#undef GROUP_DEF
    console() << "Reads from " << configPath.string() << endl;
}

void writeConfig()
{
#ifdef CINDER_COCOA_TOUCH
    // TODO: how to enable writing in iOS?
    return;
#endif

    fs::path configPath = app::getAppPath() / CONFIG_XML;
    try
    {
        XmlTree tree = XmlTree::createDoc();
        XmlTree group;

#define GROUP_DEF(grp)                                          \
        do                                                      \
        {                                                       \
            if (!group.getTag().empty()) tree.push_back(group); \
            group = XmlTree(#grp, "");                          \
        } while (0);

#define ITEM_DEF(type, var, default)                            \
        do                                                      \
        {                                                       \
            XmlTree item(#var, toString(var));                  \
            if (group.getTag().empty()) tree.push_back(item);   \
            else group.push_back(item);                         \
        } while (0);
#define ITEM_DEF_MINMAX(type, var, default, Min, Max) ITEM_DEF(type, var, default);
#include ITEM_DEF_FILE
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF
#undef GROUP_DEF
        if (!group.getTag().empty()) tree.push_back(group);

#ifdef CHINESE_GBK_ENCODING_ENABLED
        // OStreamRef os = writeFile(configPath)->getStream();
        ofstream of(configPath.string().c_str());
        const string kGbkHeader = "<?xml version=\"1.0\" encoding=\"gbk\"?>";
        of << kGbkHeader << endl << tree;
#else
        tree.write( writeFile(configPath));
#endif
        console() << "Writes to " << configPath.string() <<endl;
    }
    catch( ... ) {
        console() << "[Warning] Fails to write to " << configPath.string() <<endl;
    }
}

void revertToDefaultValues()
{
#define GROUP_DEF(grp)
#define ITEM_DEF(type, var, default) var = default;
#define ITEM_DEF_MINMAX(type, var, default, Min, Max) ITEM_DEF(type, var, default);
#include ITEM_DEF_FILE
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF
#undef GROUP_DEF
    console() << "MiniConfig reverted to default values" << endl;
}

namespace
{
    const int kPODItemHeight = 16;
    template <typename T>
    int getItemHeight(const T&)
    {
        return kPODItemHeight;
    }

    template <>
    int getItemHeight(const quat&)
    {
        return 80;
    }

    template <>
    int getItemHeight(const vec3&)
    {
        return 120;
    }

    template <>
    int getItemHeight(const Color&)
    {
        return 80;
    }

    template <>
    int getItemHeight(const ColorA&)
    {
        return 80;
    }

    int getConfigUIHeight()
    {
        int height = kPODItemHeight * 4; // top + bottom

    #define GROUP_DEF(grp)                  height += kPODItemHeight;
    #define ITEM_DEF(type, var, default)    height += getItemHeight(var);
    #define ITEM_DEF_MINMAX(type, var, default, Min, Max) ITEM_DEF(type, var, default);
    #include ITEM_DEF_FILE
    #undef ITEM_DEF_MINMAX
    #undef ITEM_DEF
    #undef GROUP_DEF

        return height;
    }
}

void takeScreenShot()
{
    auto windowSurf = copyWindowSurface();
#ifdef CINDER_COCOA_TOUCH
    cocoa::writeToSavedPhotosAlbum(windowSurf);
#else
    fs::path writePath = getAppPath() / ("screenshot_" + toString(getElapsedFrames()) + ".png");
    writeImage(writePath, windowSurf);
#endif
}

shared_ptr<params::InterfaceGl> createConfigUI(const ivec2& size)
{
#if defined(CINDER_MSW_DESKTOP) || defined(CINDER_LINUX) || defined(CINDER_MAC)
    ivec2 newsize = {size.x, max(size.y, getConfigUIHeight()) };
    
    auto params = params::InterfaceGl::create("MiniConfig.xml", newsize);
    params->addButton("SAVE", writeConfig);
    params->addButton("PROFILER", [] {
        launchWebBrowser(Url(getAssetPath("vis/index.html").string(), true));
        });
    params->addButton("SCREENSHOT", takeScreenShot);
    params->addButton("QUIT", []{App::get()->quit(); });
    params->setOptions("", "valueswidth=fit");
    params->addSeparator();
    string groupName = "Config";
#define GROUP_DEF(grp)                  groupName = #grp;
#define ITEM_DEF(type, var, default)    params->addParam(#var, &var, #var[0] == '_').group(groupName);
#define ITEM_DEF_MINMAX(type, var, default, Min, Max)       \
do                                                          \
{                                                           \
type step = (Max - Min) / (type)500;                        \
params->addParam(#var, &var).min(Min).max(Max).step(step).group(groupName);  \
} while(0);
#include ITEM_DEF_FILE
#undef ITEM_DEF_MINMAX
#undef ITEM_DEF
#undef GROUP_DEF
    params->addSeparator();
    
    getWindow()->getSignalPostDraw().connect([params] {
#if defined( CINDER_MSW_DESKTOP )
        gl::pushDebugGroup("MiniConfig::UI");
#endif
        params->draw();
#if defined( CINDER_MSW_DESKTOP )
        gl::popDebugGroup();
#endif
    });
    
    return params;
#else
    return nullptr;
#endif
}

