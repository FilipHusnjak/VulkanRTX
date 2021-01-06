#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class PerspectiveCamera
{
  public:
    PerspectiveCamera(float fovY, float aspect, float zNear, float zFar);

    void SetProjection(float fovY, float aspect, float zNear, float zFar);

    void SetPosition(const glm::vec3 &eye)
    {
        m_Position = eye;
        RecalculateViewMatrix();
    }

    void SetPosition(const glm::vec3 &eye, const glm::vec3 &center)
    {
        m_Position = eye;
        m_Front = glm::normalize(center - eye);
        RecalculateViewMatrix();
    }

    const glm::vec3 &GetPosition() const
    {
        return m_Position;
    }

    void Translate(const glm::vec3 &translation)
    {
        m_Position += translation;
        RecalculateViewMatrix();
    }

    void SetAngles(const float yaw, const float pitch)
    {
        m_Front.x = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        m_Front.y = sin(glm::radians(pitch));
        m_Front.z = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        m_Front = glm::normalize(m_Front);
        RecalculateViewMatrix();
    }

    void Rotate(const float yaw_, const float pitch_)
    {
        float yaw = m_Front.z >= 0
                        ? asinf(m_Front.x / sqrt(m_Front.x * m_Front.x + m_Front.z * m_Front.z)) +
                              glm::radians(yaw_)
                        : -asinf(m_Front.x / sqrt(m_Front.x * m_Front.x + m_Front.z * m_Front.z)) +
                              glm::radians(yaw_) - glm::pi<float>();
        float pitch = asinf(m_Front.y / glm::length(m_Front)) + glm::radians(pitch_);
        if (pitch > glm::radians(89.0f))
            pitch = glm::radians(89.0f);
        if (pitch < -glm::radians(89.0f))
            pitch = -glm::radians(89.0f);
        m_Front.x = sin(yaw) * cos(pitch);
        m_Front.y = sin(pitch);
        m_Front.z = cos(yaw) * cos(pitch);
        m_Front = glm::normalize(m_Front);
        RecalculateViewMatrix();
    }

    const glm::mat4 &GetProjectionMatrix() const
    {
        return m_ProjectionMatrix;
    }

    const glm::mat4 &GetViewMatrix() const
    {
        return m_ViewMatrix;
    }

    const glm::vec3 &GetFront() const
    {
        return m_Front;
    }

    const glm::vec3 &GetUp() const
    {
        return m_Up;
    }

  private:
    void RecalculateViewMatrix();

  private:
    glm::mat4 m_ProjectionMatrix;
    glm::mat4 m_ViewMatrix;

    glm::vec3 m_Position;
    glm::vec3 m_Front;
    glm::vec3 m_Up = {0.0f, 1.0f, 0.0f};
};