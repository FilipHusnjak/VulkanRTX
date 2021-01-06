#pragma once

#include "Core/LayerStack.h"
#include "Event/ApplicationEvent.h"
#include "Event/Event.h"
#include "Window/WindowsWindow.h"
#include "Core/ImGuiLayer.h"

#include <chrono>

class Application
{
  public:
    Application();
    ~Application();
    Application(const Application &other) = delete;
    Application &operator=(const Application &other) = delete;
    void Run();
    void OnEvent(Event &e);
    void PushLayer(Layer *layer);
    void PushOverlay(Layer *layer);

    const inline WindowsWindow &GetWindow() const
    {
        return m_Window;
    }

    static Application &Get()
    {
        return *s_Instance;
    }

  private:
    bool OnWindowClose(WindowCloseEvent &e);
    bool OnWindowResize(WindowResizeEvent &e);

  private:
    static Application *s_Instance;

    WindowsWindow m_Window;
    bool m_Running = true;
    bool m_Minimized = false;
    ImGuiLayer *m_ImGuiLayer;
    LayerStack m_LayerStack;
    std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime =
        std::chrono::high_resolution_clock::now();
};