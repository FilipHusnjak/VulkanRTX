#pragma once

#include "Core/Layer.h"

#include "Renderer/ObjModel.h"

#include "Renderer/PerspectiveCameraController.h"

class Sandbox3D : public Layer
{
  public:
    Sandbox3D();
    ~Sandbox3D() = default;

    void OnAttach() override;
    void OnDetach() override;

    void OnUpdate(float ts) override;
    void OnImGuiRender() override;
    void OnEvent(Event &e) override;

  private:
    PerspectiveCameraController m_CameraController;
    std::vector<ObjInstance> m_Instances;
    std::vector<ObjModel> m_Models;

    std::queue<float> m_Times;
    float m_TimePassed = 0.0f;
    int m_FrameCount = 0;
};