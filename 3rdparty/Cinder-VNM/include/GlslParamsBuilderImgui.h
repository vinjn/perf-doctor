#pragma once

#include "cinder/gl/GlslProg.h"
#include "MiniConfigImgui.h"

#include <unordered_map>

struct GlslParamsBuilderImgui
{
    GlslParamsBuilderImgui() {}
    
    GlslParamsBuilderImgui(ci::gl::GlslProgRef glsl)
    {
        glslProg = glsl;
        if (!glslProg) return;
        //
        // glsl reflection: uniform
        //
        activeUniforms = glsl->getActiveUniforms();
        sort(activeUniforms.begin(), activeUniforms.end(), [](const ci::gl::GlslProg::Uniform & a, const ci::gl::GlslProg::Uniform & b) -> bool{
            return a.getName() < b.getName();
        });
        
        connection = App::get()->getSignalUpdate().connect(std::bind(&GlslParamsBuilderImgui::drawImgui, this));
    }
    
    ~GlslParamsBuilderImgui()
    {
        connection.disconnect();
    }
    
    void applyUniforms()
    {
        if (!glslProg) return;

        for (auto& kv : namedInts) glslProg->uniform(kv.first, kv.second);
        for (auto& kv : namedBools) glslProg->uniform(kv.first, kv.second);
        for (auto& kv : namedFloats) glslProg->uniform(kv.first, kv.second);
        for (auto& kv : namedVec2s) glslProg->uniform(kv.first, kv.second);
        for (auto& kv : namedVec3s) glslProg->uniform(kv.first, kv.second);
        for (auto& kv : namedVec4s) glslProg->uniform(kv.first, kv.second);
    }
    
    ci::gl::GlslProgRef glslProg;
    std::vector<gl::GlslProg::Uniform> activeUniforms;
    
    std::unordered_map<std::string, int> namedInts;
    std::unordered_map<std::string, bool> namedBools;
    std::unordered_map<std::string, float> namedFloats;
    std::unordered_map<std::string, ci::vec2> namedVec2s;
    std::unordered_map<std::string, ci::vec3> namedVec3s;
    std::unordered_map<std::string, ci::vec4> namedVec4s;
    
private:
    
    ci::signals::Connection connection;
    
    template <typename T>
    void addParam(ci::gl::GlslProgRef glsl,
                  std::string guiName, std::string name, std::unordered_map<std::string, T>& container)
    {
        auto& value = container[name];
        if (vnm::addImguiParam(guiName.c_str(), value))
        {
            glsl->uniform(name, value);
        };
    }
    
    void drawImgui()
    {
        ui::ScopedWindow window("Config");
        
        auto label = glslProg->getLabel();
        if (!ui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            return;
        
        for (auto uniform : activeUniforms)
        {
            if (uniform.getCount() != 1) continue; // skip array
            if (uniform.getUniformSemantic() != ci::gl::UNIFORM_USER_DEFINED) continue; // skip cinder-defined semantic
            auto name = uniform.getName();
            auto guiName = name + " @ " + label;
            
            switch (uniform.getType())
            {
                case GL_INT:
                case GL_SAMPLER_2D:
                case GL_SAMPLER_CUBE:
                {
                    addParam(glslProg, guiName, name, namedInts);
                    break;
                }
                case GL_FLOAT:
                {
                    addParam(glslProg, guiName, name, namedFloats);
                    break;
                }
                case GL_FLOAT_VEC2:
                {
                    addParam(glslProg, guiName, name, namedVec2s);
                    break;
                }
                case GL_FLOAT_VEC3:
                {
                    addParam(glslProg, guiName, name, namedVec3s);
                    break;
                }
                case GL_FLOAT_VEC4:
                {
                    addParam(glslProg, guiName, name, namedVec4s);
                    break;
                }
                case GL_BOOL:
                {
                    addParam(glslProg, guiName, name, namedBools);
                    break;
                }
                default:
                {
                    ui::Text("%s", guiName.c_str());
                    break;
                }
            }
        }
        
    };
    
};

