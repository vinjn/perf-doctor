#include "../include/AssetManager.h"
#include "cinder/app/App.h"
#include "cinder/ImageIo.h"
#include "cinder/Function.h"
#include "cinder/Utilities.h"
#include "cinder/ObjLoader.h"
#include "cinder/Log.h"
#include "cinder/Function.h"
#include "cinder/GeomIo.h"
#include "cinder/gl/Shader.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Sync.h"
#include "cinder/ip/Checkerboard.h"
#include "cinder/ConcurrentCircularBuffer.h"

#include <map>

using namespace std;
using namespace ci;
using namespace app;

static bool sShouldQuit = false;
template <typename T>
T& getAssetResource(const string& relativeName, function<T(const string&, const string&)> loadFunc, const string& relativeNameB = "")
{
    typedef map<string, T> MapType;
    static MapType sMap;
    static T nullResource;
    auto it = sMap.find(relativeName + relativeNameB);
    if (it != sMap.end())
    {
        return it->second;
    }

    static std::once_flag connectCloseSignal;
    auto fn = [] {
        App::get()->getSignalCleanup().connect([] {
            sShouldQuit = true;
            sMap.clear();
        });
    };
    std::call_once(connectCloseSignal, fn);

    fs::path aPath, bPath;
    if (!relativeName.empty())
    {
        aPath = getAssetPath(relativeName);
    }
    if (aPath.empty()) aPath = relativeName;
    if (!relativeNameB.empty())
    {
        bPath = getAssetPath(relativeNameB);
    }
    if (bPath.empty()) bPath = relativeNameB;

    try
    {
        CI_LOG_I("Loading: " << relativeName << " " << relativeNameB);
        auto resource = loadFunc(aPath.string(), bPath.string());
        return sMap[relativeName + relativeNameB] = resource;
    }
    catch (Exception& e)
    {
        CI_LOG_E(cinder::System::demangleTypeName(typeid(e).name()) << ", reason: \n" << e.what());
        sMap[relativeName + relativeNameB] = nullResource;
    }
    return nullResource;
}

namespace am
{
    void addAssetDirectory(const fs::path& dirPath)
    {
        if (fs::exists(dirPath))
            app::addAssetDirectory(dirPath);
    }

    BufferRef& buffer(const string& relativeName)
    {
        auto loader = [](const string & absoluteName, const string&) -> BufferRef
        {
            auto source = loadFile(absoluteName);
            return source->getBuffer();
        };
        return getAssetResource<BufferRef>(relativeName, loader);
    }

    SurfaceRef& surface(const string& relativeName, bool forceAlpha)
    {
        auto loader = [forceAlpha](const string & absoluteName, const string&) -> SurfaceRef
        {
            auto source = loadImage(absoluteName);
            bool hasAlpha = source->hasAlpha() || forceAlpha;
            return Surface::create(source, SurfaceConstraintsDefault(), hasAlpha);
        };
        return getAssetResource<SurfaceRef>(relativeName, loader);
    }

    Surface16uRef& surface16u(const string& relativeName, bool forceAlpha)
    {
        auto loader = [forceAlpha](const string& absoluteName, const string&) -> Surface16uRef
        {
            auto source = loadImage(absoluteName);
            bool hasAlpha = source->hasAlpha() || forceAlpha;
            return Surface16u::create(source, SurfaceConstraintsDefault(), hasAlpha);
        };
        return getAssetResource<Surface16uRef>(relativeName, loader);
    }

    Surface32fRef& surface32f(const string& relativeName, bool forceAlpha)
    {
        auto loader = [forceAlpha](const string& absoluteName, const string&) -> Surface32fRef
        {
            auto source = loadImage(absoluteName);
            bool hasAlpha = source->hasAlpha() || forceAlpha;
            return Surface32f::create(source, SurfaceConstraintsDefault(), hasAlpha);
        };
        return getAssetResource<Surface32fRef>(relativeName, loader);
    }

    ChannelRef& channel(const std::string& relativeName)
    {
        auto loader = [](const string & absoluteName, const string&) -> ChannelRef
        {
            auto source = loadImage(absoluteName);
            return Channel::create(source);
        };
        return getAssetResource<ChannelRef>(relativeName, loader);
    }

    Channel16uRef& channel16u(const std::string& relativeName)
    {
        auto loader = [](const string & absoluteName, const string&) -> Channel16uRef
        {
            auto source = loadImage(absoluteName);
            return Channel16u::create(source);
        };
        return getAssetResource<Channel16uRef>(relativeName, loader);
    }

    template <typename T>
    shared_ptr<T>& texture(const string& relativeName, const typename T::Format& format, bool isAsync)
    {
        int kCircularTextureCount = 20;
        struct Task
        {
            string texFilename;
        };

        static ConcurrentCircularBuffer<Task> tasks(kCircularTextureCount);
        static unique_ptr<thread> textureLoader;

        auto _format = format;
        _format.setLabel(relativeName);
        auto loader = [=](const string & absoluteName, const string&) -> shared_ptr < T >
        {
            if (absoluteName == "checkerboard")
            {
                auto source = ip::checkerboard(2048, 2048);
                return T::create(source, _format);
            }
            if (!fs::exists(absoluteName))
            {
                CI_LOG_E("Missing file: " << absoluteName);
                auto source = ip::checkerboard(512, 512);
                return T::create(source, _format);
            }

            auto ext = fs::path(absoluteName).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), static_cast<int(*)(int)>(tolower));
#if !defined( CINDER_GL_ES ) || defined( CINDER_GL_ANGLE )
            if (ext == ".dds")
            {
                auto source = DataSourcePath::create(absoluteName);
                return T::createFromDds(source, _format);
            }
#endif
            if (ext == ".ktx")
            {
                auto source = DataSourcePath::create(absoluteName);
                return T::createFromKtx(source, _format);
            }
            auto source = loadImage(absoluteName);
            return T::create(source, _format);
        };

        // TODO: use call_once
        if (isAsync)
        {
            if (!textureLoader)
            {
                gl::ContextRef backgroundCtx = gl::Context::create(gl::context());

                auto fn = [=](gl::ContextRef context) {
                    context->makeCurrent();
                    while (!sShouldQuit)
                    {
                        Task task;
                        if (tasks.tryPopBack(&task))
                        {
                            auto newTex = loader(task.texFilename, "");

                            // we need to wait on a fence before alerting the primary thread that the Texture is ready
                            auto fence = gl::Sync::create();
                            fence->clientWaitSync();

                            texture<T>(task.texFilename, format, true) = newTex;
                        }
                    }
                };
                textureLoader = unique_ptr<thread>(new thread(bind(fn, backgroundCtx)));

                App::get()->getSignalCleanup().connect([] {
                    textureLoader->join();
                });
            }

            auto fakeLoader = [=](const string & absoluteName, const string&) -> shared_ptr < T > {
                static auto placeholder = ip::checkerboard(64, 64);
                static auto tex = T::create(placeholder, _format);
                tasks.pushFront({ absoluteName });

                return tex;
            };

            return getAssetResource<shared_ptr<T>>(relativeName, fakeLoader);
        }

        return getAssetResource<shared_ptr<T>>(relativeName, loader);
    }

#if ! defined( CINDER_GL_ES )
    //gl::Texture1dRef& texture1d(const std::string& relativeName, const gl::Texture1d::Format& format, bool isAsync)
    //{
    //    return texture<gl::Texture1d>(relativeName, format, isAsync);
    //}
#endif

    gl::Texture2dRef& texture2d(const std::string& relativeName, const gl::Texture2d::Format& format, bool isAsync)
    {
        return texture<gl::Texture2d>(relativeName, format, isAsync);
    }

#if TEXTURE_3D_ENABLED
    gl::Texture3dRef& texture3d(const std::string& relativeName, const gl::Texture3d::Format& format, bool isAsync)
    {
        return texture<gl::Texture3d>(relativeName, format, isAsync);
    }
#endif

    gl::TextureCubeMapRef& textureCubeMap(const std::string& relativeName, const gl::TextureCubeMap::Format& format, bool isAsync)
    {
        return texture<gl::TextureCubeMap>(relativeName, format, isAsync);
    }

    TriMeshRef& triMesh(const string& objFileName, const std::string& mtlFileName)
    {
        auto loader = [](const string & absObjFileName, const string& absMtlFileName) -> TriMeshRef
        {
#define ENTRY(name)  if (absObjFileName == #name) return TriMesh::create(geom::name());
            ENTRY(Rect);
            ENTRY(RoundedRect);
            ENTRY(Cube);
            ENTRY(Icosahedron);
            ENTRY(Icosphere);
            ENTRY(Teapot);
            ENTRY(Circle);
            ENTRY(Ring);
            ENTRY(Sphere);
            ENTRY(Capsule);
            ENTRY(Torus);
            ENTRY(TorusKnot);
            ENTRY(Cylinder);
            ENTRY(Plane);
            ENTRY(WireCapsule);
            ENTRY(WireCircle);
            ENTRY(WireRoundedRect);
            ENTRY(WireCube);
            ENTRY(WireCylinder);
            ENTRY(WireCone);
            ENTRY(WireIcosahedron);
            ENTRY(WirePlane);
            ENTRY(WireSphere);
            ENTRY(WireTorus);
#undef ENTRY
            auto source = DataSourcePath::create(absObjFileName);
            auto ext = fs::path(absObjFileName).extension().string();
            TriMeshRef mesh;

            std::transform(ext.begin(), ext.end(), ext.begin(), static_cast<int(*)(int)>(tolower));
            if (ext == ".obj")
            {
                if (absMtlFileName.empty())
                {
                    ObjLoader loader(source);
                    mesh = TriMesh::create(loader);
                }
                else
                {
                    auto mtlSource = DataSourcePath::create(absMtlFileName);
                    ObjLoader loader(source, mtlSource);
                    mesh = TriMesh::create(loader);
                }
            }
            else if (ext == ".msh")
            {
                mesh = TriMesh::create();
                mesh->read(source);
            }
            else
            {
                CI_LOG_E("Unsupported mesh format: " << absObjFileName);
                return nullptr;
            }

            if (!mesh->hasNormals()) {
                mesh->recalculateNormals();
            }

            if (!mesh->hasTangents()) {
                mesh->recalculateTangents();
            }

            return mesh;
        };
        return getAssetResource<TriMeshRef>(objFileName, loader, mtlFileName);
    }

    gl::VboMeshRef& vboMesh(const string& objFileName, const string& mtlFileName)
    {
        auto tri = triMesh(objFileName, mtlFileName);
        auto loader = [&tri](const string & absoluteName, const string&) -> gl::VboMeshRef
        {
            return gl::VboMesh::create(*tri);
        };

        return getAssetResource<gl::VboMeshRef>(objFileName, loader, mtlFileName);
    }

    gl::GlslProgRef& glslProg(const string& vsFileName, const string& fsFileName, gl::GlslProg::Format format)
    {
        auto label = fs::path(vsFileName).filename().string() + "/" + fs::path(fsFileName).filename().string();
        auto loader = [=, &format](const string & vsAbsoluteName, const string & fsAbsoluteName) -> gl::GlslProgRef
        {
            if (vsAbsoluteName == fsAbsoluteName || fsAbsoluteName.empty())
            {
                // Assume it's a stock shader
                auto def = gl::ShaderDef();
                bool isStockShader = false;
                if (vsAbsoluteName.find("passthrough") != string::npos) {
                    isStockShader = true;
                }
                if (vsAbsoluteName.find("texture") != string::npos) {
                    isStockShader = true; def = def.texture();
                }
                if (vsAbsoluteName.find("color") != string::npos) {
                    isStockShader = true;  def = def.color();
                }
                if (vsAbsoluteName.find("lambert") != string::npos) {
                    isStockShader = true; def = def.lambert();
                }
                if (vsAbsoluteName.find("uniform") != string::npos) {
                    isStockShader = true; def = def.uniformBasedPosAndTexCoord();
                }

                if (isStockShader)
                    return gl::getStockShader(def);
            }

#if defined( CINDER_GL_ES )
            format.version(300); // es 3.0
#else
            format.version(150); // gl 3.2
#endif
            if (!vsAbsoluteName.empty())
                format.vertex(DataSourcePath::create(vsAbsoluteName));
            if (!fsAbsoluteName.empty())
                format.fragment(DataSourcePath::create(fsAbsoluteName));
            format.setLabel(label);

            return gl::GlslProg::create(format);
        };

        return getAssetResource<gl::GlslProgRef>(vsFileName, loader, fsFileName);
    }

    string& str(const string& relativeName)
    {
        auto loader = [](const string & absoluteName, const string&) -> string
        {
            return loadString(DataSourcePath::create(absoluteName));
        };
        return getAssetResource<string>(relativeName, loader);
    }

    static vector<string> loadPaths(const string& absoluteFolderName, const string&, bool isLongMode)
    {
        vector<string> files;
        fs::directory_iterator kEnd;
        for (fs::directory_iterator it(absoluteFolderName); it != kEnd; ++it)
        {
            if (fs::is_regular_file(*it) && it->path().extension() != ".db"
                && it->path().extension() != ".DS_Store")
            {
#ifndef NDEBUG
                //console() << it->path() << endl;
#endif
                files.push_back(isLongMode ?
                    it->path().string() :
                    it->path().filename().string());
            }
        }
        return files;
    }

    vector<string>& longPaths(const string& relativeFolderName)
    {
        return getAssetResource<vector<string>>(relativeFolderName, bind(loadPaths, placeholders::_1, placeholders::_2, true));
    }

    vector<string>& shortPaths(const string& relativeFolderName)
    {
        return getAssetResource<vector<string>>(relativeFolderName, bind(loadPaths, placeholders::_1, placeholders::_2, false));
    }

    audio::VoiceRef& voice(const string& relativeName)
    {
#if defined(CINDER_MSW)
        auto loader = [](const string & absoluteName, const string&) -> audio::VoiceRef
        {
            auto source = audio::load(DataSourcePath::create(absoluteName));
            return audio::Voice::create(source);
        };
        return getAssetResource<audio::VoiceRef>(relativeName, loader);
#else
        throw audio::AudioExc("ci::audio is unsupported");
#endif
    }

}
