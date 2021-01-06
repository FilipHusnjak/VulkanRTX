#pragma once

#include "Event/ApplicationEvent.h"
#include "Event/Event.h"

#include <GLFW/glfw3.h>

#define WIDTH 1920

#define HEIGHT 1080

class WindowsWindow
{
  public:
    using EventCallbackFn = std::function<void(Event &)>;

    WindowsWindow();
    ~WindowsWindow();

    void OnUpdate();

    bool WindowClose();

    inline bool Resized() const
    {
        return m_Data.Resized;
    }

    void ResetResized();

    inline unsigned int GetWidth() const
    {
        return m_Data.Width;
    }

    inline unsigned int GetHeight() const
    {
        return m_Data.Height;
    }

    inline GLFWwindow *GetNativeWindow() const
    {
        return m_Window;
    }

    inline void SetEventCallback(const EventCallbackFn &callback)
    {
        m_Data.EventCallback = callback;
    }

  private:
    void Init();
    void Shutdown();

  private:
    GLFWwindow *m_Window;

    struct WindowData
    {
        std::string Title;
        unsigned int Width, Height;
        EventCallbackFn EventCallback;
        bool Resized = false;
    };

    WindowData m_Data;
};
