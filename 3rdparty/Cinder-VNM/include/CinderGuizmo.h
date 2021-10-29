#pragma once

#include <glm/mat4x4.hpp>

namespace ImGui
{
    void EnableGizmo(bool enable);
    bool EditGizmo(const glm::mat4& cameraView, const glm::mat4& cameraProjection, glm::mat4* matrix);
    void DecomposeMatrixToComponents(const glm::mat4& transform, glm::vec3& matrixTranslation, glm::vec3& matrixRotation, glm::vec3& matrixScale);
    bool ViewManipulate(glm::mat4& cameraView, float length, const glm::ivec2& position, const glm::ivec2& size = { 128, 128 }, uint32_t backgroundColor = 0x10101010);
}