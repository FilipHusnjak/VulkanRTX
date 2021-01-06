#include "vkpch.h"

#include "WindowsWindow.h"

#include "Event/KeyEvent.h"

WindowsWindow::WindowsWindow() : m_Data{"RTX_ON", WIDTH, HEIGHT}
{
    Init();
}

WindowsWindow::~WindowsWindow()
{
    Shutdown();
}

void WindowsWindow::OnUpdate()
{
    glfwPollEvents();
}

bool WindowsWindow::WindowClose()
{
    return glfwWindowShouldClose(m_Window);
}

void WindowsWindow::ResetResized()
{
    m_Data.Resized = false;
}

void WindowsWindow::Init()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = glfwCreateWindow(WIDTH, HEIGHT, m_Data.Title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, &this->m_Data);
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow *window, int width, int height) {
        auto data = reinterpret_cast<WindowData *>(glfwGetWindowUserPointer(window));
        data->Width = width;
        data->Height = height;
        data->Resized = true;
        WindowResizeEvent event(width, height);
        data->EventCallback(event);
    });
    glfwSetWindowCloseCallback(m_Window, [](GLFWwindow *window) {
        auto data = reinterpret_cast<WindowData *>(glfwGetWindowUserPointer(window));
        WindowCloseEvent event;
        data->EventCallback(event);
    });
    glfwSetKeyCallback(m_Window,
                       [](GLFWwindow *window, int key, int scancode, int action, int mods) {
                           WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

                           switch (action)
                           {
                           case GLFW_PRESS: {
                               KeyPressedEvent event(key);
                               data.EventCallback(event);
                               break;
                           }
                           case GLFW_RELEASE: {
                               KeyReleasedEvent event(key);
                               data.EventCallback(event);
                               break;
                           }
                           }
                       });
}

void WindowsWindow::Shutdown()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}
