#include "vkpch.h"

#include "VulkanTools.h"

#include "Core/Core.h"

bool CheckExtensionSupport(uint32_t glfwExtensionCount, const char *const *glfwExtensions)
{
    std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
    for (uint32_t i = 0; i < glfwExtensionCount; i++, glfwExtensions++)
    {
        bool found = false;
        for (const auto &extension : extensions)
        {
            if (strcmp(extension.extensionName, *glfwExtensions))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

bool CheckValidationLayerSupport()
{
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

    for (auto layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice &physicalDevice,
                                     const vk::SurfaceKHR &surface)
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();

    size_t graphicsQueueFamilyIndex =
        std::distance(queueFamilyProperties.begin(),
                      std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                                   [](vk::QueueFamilyProperties const &qfp) {
                                       return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                                   }));

    if (graphicsQueueFamilyIndex == queueFamilyProperties.size())
    {
        return {};
    }

    size_t presentQueueFamilyIndex = physicalDevice.getSurfaceSupportKHR(
                                         static_cast<uint32_t>(graphicsQueueFamilyIndex), surface)
                                         ? graphicsQueueFamilyIndex
                                         : queueFamilyProperties.size();

    if (presentQueueFamilyIndex == queueFamilyProperties.size())
    {
        for (size_t i = 0; i < queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface))
            {
                graphicsQueueFamilyIndex = static_cast<uint32_t>(i);
                presentQueueFamilyIndex = i;
                break;
            }
        }
        if (presentQueueFamilyIndex == queueFamilyProperties.size())
        {
            for (size_t i = 0; i < queueFamilyProperties.size(); i++)
            {
                if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface))
                {
                    presentQueueFamilyIndex = i;
                    break;
                }
            }
        }
    }

    if (presentQueueFamilyIndex == queueFamilyProperties.size())
    {
        return {};
    }

    return {static_cast<uint32_t>(graphicsQueueFamilyIndex),
            static_cast<uint32_t>(presentQueueFamilyIndex)};
}

bool CheckDeviceExtensionSupport(vk::PhysicalDevice physicalDevice)
{
    std::vector<vk::ExtensionProperties> availableExtensions =
        physicalDevice.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice physicalDevice,
                                              vk::SurfaceKHR surface)
{
    SwapChainSupportDetails details{};
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return details;
}

bool IsDeviceSuitable(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface)
{
    QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
    bool extensionsSupported = CheckDeviceExtensionSupport(physicalDevice);
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice, surface);
        swapChainAdequate =
            !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();

    return indices.IsComplete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

vk::Format FindSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features, vk::PhysicalDevice &physicalDevice)
{
    for (auto &format : candidates)
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == vk::ImageTiling::eOptimal &&
                 (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Format not found");
}

vk::Format FindDepthFormat(vk::PhysicalDevice &physicalDevice)
{
    return FindSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment,
        physicalDevice);
}

uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                        vk::PhysicalDevice physicalDevice)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if (typeFilter & (1 << i) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
