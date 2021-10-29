#include "CinderGuizmo.h"
#include "cinder/app/KeyEvent.h"
#include "imgui/imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"
#include "glm/gtc/type_ptr.hpp"
#include "ImGuizmo/ImGuizmo.h"

using namespace ci;
using namespace app;

void ImGui::EnableGizmo(bool enable)
{
    ImGuizmo::Enable(enable);
}

bool ImGui::EditGizmo(const glm::mat4& cameraView, const glm::mat4& cameraProjection, glm::mat4* matrix)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
    static bool useSnap = false;
    static float snap[3] = { 1.f, 1.f, 1.f };
    static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
    static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
    static bool boundSizing = false;
    static bool boundSizingSnap = false;

    ImGuizmo::BeginFrame();

    if (ImGui::IsKeyPressed(KeyEvent::KEY_t))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(KeyEvent::KEY_r))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(KeyEvent::KEY_s))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(*matrix), matrixTranslation, matrixRotation, matrixScale);
    bool trsDirty = false;
    trsDirty |= ImGui::InputFloat3("Tr", matrixTranslation, 3);
    trsDirty |= ImGui::InputFloat3("Rt", matrixRotation, 3);
    trsDirty |= ImGui::InputFloat3("Sc", matrixScale, 3);
    ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, glm::value_ptr((*matrix)));

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
    }
    if (ImGui::IsKeyPressed(KeyEvent::KEY_z))
        useSnap = !useSnap;
    ImGui::Checkbox("", &useSnap);
    ImGui::SameLine();

    switch (mCurrentGizmoOperation)
    {
    case ImGuizmo::TRANSLATE:
        ImGui::InputFloat3("Snap", &snap[0]);
        break;
    case ImGuizmo::ROTATE:
        ImGui::InputFloat("Angle Snap", &snap[0]);
        break;
    case ImGuizmo::SCALE:
        ImGui::InputFloat("Scale Snap", &snap[0]);
        break;
    }
    #if 0
    ImGui::Checkbox("Bound Sizing", &boundSizing);
    if (boundSizing)
    {
        ImGui::PushID(3);
        ImGui::Checkbox("", &boundSizingSnap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", boundsSnap);
        ImGui::PopID();
    }
    #endif

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), mCurrentGizmoOperation, mCurrentGizmoMode, glm::value_ptr(*matrix),
    NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

    return trsDirty || ImGuizmo::IsUsing();
}

bool ImGui::ViewManipulate(glm::mat4& cameraView, float length, const glm::ivec2& position, const glm::ivec2& size, uint32_t backgroundColor)
{
    ImGuiIO& io = ImGui::GetIO();
    auto position_ = ImVec2(position.x, position.y);
    auto size_ = ImVec2(size.x, size.y);
    bool isInside = ImRect(position_, position_ + size_).Contains(io.MousePos);

    ImGuizmo::ViewManipulate(glm::value_ptr(cameraView), length, position_, size_, backgroundColor);

    return isInside;
}

void ImGui::DecomposeMatrixToComponents(const glm::mat4& transform, glm::vec3& matrixTranslation, glm::vec3& matrixRotation, glm::vec3& matrixScale)
{
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), glm::value_ptr(matrixTranslation), glm::value_ptr(matrixRotation), glm::value_ptr(matrixScale));
}
