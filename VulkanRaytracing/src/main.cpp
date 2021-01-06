#include "vkpch.h"

#include "Renderer/VulkanRenderer.h"
#include "Window/WindowsWindow.h"
#include "Application.h"

int main()
{
    Application *app = new Application();
    app->Run();
    delete app;
    return 0;
}