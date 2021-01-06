#include "vkpch.h"

#include "PerspectiveCameraController.h"

#include "Core/Input.h"

#include <GLFW/glfw3.h>

PerspectiveCameraController::PerspectiveCameraController(float aspectRatio, bool rotation)
    : m_Camera(glm::radians(45.0f), aspectRatio, 0.1f, 6000.0f), m_AspectRatio(aspectRatio),
      m_Rotation(rotation)
{
    m_Camera.SetPosition({-6, 0, -5}, {-5, 0, 0});
}

void PerspectiveCameraController::OnUpdate(float ts)
{
    if (Input::IsKeyPressed(GLFW_KEY_W))
        m_Camera.Translate(m_CameraSpeed * m_Camera.GetFront() * ts);
    if (Input::IsKeyPressed(GLFW_KEY_S))
        m_Camera.Translate(-m_CameraSpeed * m_Camera.GetFront() * ts);
    if (Input::IsKeyPressed(GLFW_KEY_A))
        m_Camera.Translate(-glm::normalize(glm::cross(m_Camera.GetFront(), m_Camera.GetUp())) *
                           m_CameraSpeed * ts);
    if (Input::IsKeyPressed(GLFW_KEY_D))
        m_Camera.Translate(glm::normalize(glm::cross(m_Camera.GetFront(), m_Camera.GetUp())) *
                           m_CameraSpeed * ts);

    static float lastX = Input::GetMouseX(), lastY = Input::GetMouseY();
    float xPos = Input::GetMouseX(), yPos = Input::GetMouseY();
    if (!m_Cursor)
    {
        float yaw = lastX - xPos;
        float pitch = lastY - yPos;

        float sensitivity = 0.1f;
        yaw *= sensitivity;
        pitch *= sensitivity;

        m_Camera.Rotate(yaw, pitch);
    }
    lastX = xPos;
    lastY = yPos;
}

void PerspectiveCameraController::OnEvent(Event &e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowResizeEvent>(
        std::bind(&PerspectiveCameraController::OnWindowResized, this, std::placeholders::_1));
    dispatcher.Dispatch<KeyPressedEvent>(
        std::bind(&PerspectiveCameraController::OnKeyPressedEvent, this, std::placeholders::_1));
}

bool PerspectiveCameraController::OnWindowResized(WindowResizeEvent &e)
{
    if (e.GetWidth() == 0 || e.GetHeight() == 0)
        return false;
    m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
    m_Camera.SetProjection(glm::radians(45.0f), (float)e.GetWidth() / (float)e.GetHeight(), 0.1f,
                           100.0f);
    return false;
}

bool PerspectiveCameraController::OnKeyPressedEvent(KeyPressedEvent &e)
{
    if (e.GetKeyCode() == GLFW_KEY_ESCAPE)
    {
        m_Cursor = !m_Cursor;
        if (m_Cursor)
        {
            Input::EnableCursor();
        }
        else
        {
            Input::DisableCursor();
        }
    }

    return false;
}
