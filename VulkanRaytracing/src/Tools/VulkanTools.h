#pragma once

#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                    VK_NV_RAY_TRACING_EXTENSION_NAME,
                                                    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
                                                    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
                                                    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                                                    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
                                                    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    inline bool IsComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

bool CheckExtensionSupport(uint32_t glfwExtensionCount, const char *const *glfwExtensions);

bool CheckValidationLayerSupport();

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice &physicalDevice,
                                     const vk::SurfaceKHR &surface);

bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);

SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

bool IsDeviceSuitable(vk::PhysicalDevice &device, vk::SurfaceKHR &surface);

vk::Format FindSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features, vk::PhysicalDevice &physicalDevice);

vk::Format FindDepthFormat(vk::PhysicalDevice &physicalDevice);

uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                        vk::PhysicalDevice physicalDevice);
