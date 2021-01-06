#pragma once

#include "Event/ApplicationEvent.h"
#include "Event/Event.h"
#include "Event/KeyEvent.h"
#include "PerspectiveCamera.h"

class PerspectiveCameraController
{
  public:
    PerspectiveCameraController(float aspectRatio, bool rotation = false);

    void OnUpdate(float ts);
    void OnEvent(Event &e);

    PerspectiveCamera &GetCamera()
    {
        return m_Camera;
    }

    const PerspectiveCamera &GetCamera() const
    {
        return m_Camera;
    }

    float GetZoomLevel() const
    {
        return m_ZoomLevel;
    }

    void SetZoomLevel(float level)
    {
        m_ZoomLevel = level;
    }

  private:
    bool OnWindowResized(WindowResizeEvent &e);
    bool OnKeyPressedEvent(KeyPressedEvent &e);

  private:
    PerspectiveCamera m_Camera;
    float m_AspectRatio;
    bool m_Rotation = false;
    float m_CameraSpeed = 0.003f;
    float m_ZoomLevel = 1.0f;
    bool m_Cursor = true;
};
