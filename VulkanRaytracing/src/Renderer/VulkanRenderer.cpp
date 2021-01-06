#include "vkpch.h"

#include "VulkanRenderer.h"

#include "Core/Allocator.h"

#include "Core/Core.h"
#include "Tools/FileTools.h"

#include "Tools/VulkanTools.h"

#include "DescriptorPool.h"
#include "RenderPass.h"

#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

#define MAX_FRAMES_IN_FLIGHT 1

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

extern bool hdr;
extern float ni;
extern float F0;

struct CameraMatrices
{
    glm::vec3 cameraPos;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewInverse;
    glm::mat4 projectionInverse;
};

struct PushConstant
{
    uint32_t instanceID;
    glm::vec3 lightPosition;
    glm::vec3 lightColor;
};

struct RtPushConstant
{
    glm::vec4 clearColor;
    glm::vec3 lightPosition;
    float lightIntensity;
    int lightType;
    uint32_t nSamples;
    int hdr;
    float ni;
    float F0;
};

auto msaaSamples = vk::SampleCountFlagBits::e8;

float lightIntensity = 100.0f;
uint32_t lightType = 0;

VulkanRenderer VulkanRenderer::s_Instance;
uint32_t VulkanRenderer::s_ImageIndex = 0;
uint32_t VulkanRenderer::s_CurrentFrame = 0;

static std::vector<ObjModel> models;
static std::vector<ObjInstance> instances;
static BufferAllocation instanceBufferAlloc;

std::vector<const char *> instanceExtensions = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};

void VulkanRenderer::Init(WindowsWindow *window)
{
    s_Instance.InitRenderer(window);
}

void VulkanRenderer::Shutdown()
{
    s_Instance.m_Device.get().waitIdle();
}

void VulkanRenderer::Flush(const std::vector<ObjInstance> &instances_)
{
    instances = instances_;
    auto cmdBuf = BeginSingleTimeCommands();
    instanceBufferAlloc = Allocator::CreateDeviceLocalBuffer(
        cmdBuf, instances, vk::BufferUsageFlagBits::eStorageBuffer);
    EndSingleTimeCommands(cmdBuf);
    s_Instance.m_Device.get().waitIdle();
    s_Instance.CreateOffscreenDescriptorResources();
    s_Instance.CreateOffscreenGraphicsPipeline();
    s_Instance.CreateUniformBuffers(s_Instance.m_CameraBufferAllocations, sizeof(CameraMatrices));
    s_Instance.UpdateOffscreenDescriptorSets();
    s_Instance.IntegrateImGui();

    s_Instance.CreatePostDescriptorResources();
    s_Instance.CreatePostGraphicsPipeline();
    s_Instance.UpdatePostDescriptorSets();

    s_Instance.CreateBottomLevelAS();
    s_Instance.CreateTopLevelAS(instances);
    s_Instance.CreateRtDescriptorResources();
    s_Instance.UpdateRtDescriptorSets();
    s_Instance.CreateRtPipeline();
    s_Instance.CreateRtShaderBindingTable();
}

void VulkanRenderer::WaitIdle()
{
    s_Instance.m_Device.get().waitIdle();
}

void VulkanRenderer::Begin()
{
    s_Instance.m_Device.get().waitForFences(s_Instance.m_InFlightFences[s_CurrentFrame].get(),
                                            VK_TRUE, UINT64_MAX);
    auto result = s_Instance.m_Device.get().acquireNextImageKHR(
        s_Instance.m_SwapChain.get(), UINT64_MAX,
        s_Instance.m_ImageAvailableSemaphores[s_CurrentFrame].get(), nullptr);
    s_ImageIndex = result.value;

    if (result.result == vk::Result::eErrorOutOfDateKHR)
    {
        s_Instance.RecreateSwapChain();
        return;
    }
    else
    {
        assert(result.result == vk::Result::eSuccess ||
               result.result == vk::Result::eSuboptimalKHR);
    }

    if (s_Instance.m_ImagesInFlight[s_ImageIndex] != VK_NULL_HANDLE)
    {
        s_Instance.m_Device.get().waitForFences(s_Instance.m_ImagesInFlight[s_CurrentFrame],
                                                VK_TRUE, UINT64_MAX);
    }
    s_Instance.m_ImagesInFlight[s_ImageIndex] = s_Instance.m_InFlightFences[s_CurrentFrame].get();

    vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    s_Instance.m_CommandBuffers[s_ImageIndex].get().begin(beginInfo);
}

void VulkanRenderer::End()
{
    s_Instance.m_CommandBuffers[s_ImageIndex].get().end();

    vk::Semaphore waitSemaphores[] = {s_Instance.m_ImageAvailableSemaphores[s_CurrentFrame].get()};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {
        s_Instance.m_RenderFinishedSemaphores[s_CurrentFrame].get()};
    vk::SubmitInfo submitInfo{
        1, waitSemaphores,  waitStages, 1, &s_Instance.m_CommandBuffers[s_ImageIndex].get(),
        1, signalSemaphores};

    s_Instance.m_Device.get().resetFences({s_Instance.m_InFlightFences[s_CurrentFrame].get()});

    s_Instance.m_GraphicsQueue.submit({submitInfo},
                                      s_Instance.m_InFlightFences[s_CurrentFrame].get());

    vk::PresentInfoKHR presentInfo{1, signalSemaphores, 1, &s_Instance.m_SwapChain.get(),
                                   &s_ImageIndex};

    auto result = s_Instance.m_PresentQueue.presentKHR(&presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
        s_Instance.m_Window->Resized())
    {
        s_Instance.m_Window->ResetResized();
        s_Instance.RecreateSwapChain();
    }
    else
    {
        assert(result == vk::Result::eSuccess);
    }

    // s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::BeginScene(const PerspectiveCamera &camera)
{
    s_Instance.UpdateCameraMatrices(camera);
}

void VulkanRenderer::EndScene()
{
    std::array<vk::ClearValue, 2> clearValues = {};
    clearValues[0].color = std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    vk::RenderPassBeginInfo renderPassInfo{s_Instance.m_PostRenderPass.get(),
                                           s_Instance.m_PostFrameBuffers[s_ImageIndex].get(),
                                           {{0, 0}, s_Instance.m_Extent},
                                           static_cast<uint32_t>(clearValues.size()),
                                           clearValues.data()};
    s_Instance.m_CommandBuffers[s_ImageIndex].get().beginRenderPass(renderPassInfo,
                                                                    vk::SubpassContents::eInline);

    float aspectRatio = static_cast<float>(s_Instance.m_Extent.width) /
                        static_cast<float>(s_Instance.m_Extent.height);

    s_Instance.m_CommandBuffers[s_ImageIndex].get().pushConstants(
        s_Instance.m_PostGraphicsPipeline.GetLayout(), vk::ShaderStageFlagBits::eFragment, 0,
        sizeof(float), &aspectRatio);

    s_Instance.m_CommandBuffers[s_ImageIndex].get().bindPipeline(vk::PipelineBindPoint::eGraphics,
                                                                 s_Instance.m_PostGraphicsPipeline);
    s_Instance.m_CommandBuffers[s_ImageIndex].get().bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, s_Instance.m_PostGraphicsPipeline.GetLayout(), 0,
        s_Instance.m_PostDescriptorSets.Get(), {});
    s_Instance.m_CommandBuffers[s_ImageIndex].get().draw(3, 1, 0, 0);
    s_Instance.m_CommandBuffers[s_ImageIndex].get().endRenderPass();
}

void VulkanRenderer::Rasterize(const glm::vec4 &clearColor, const glm::vec3 &lightPosition)
{
    std::array<vk::ClearValue, 3> clearValues = {};
    memcpy(&clearValues[0].color.float32, &clearColor, sizeof(clearValues[0].color.float32));
    clearValues[1].depthStencil = {1.0f, 0};
    memcpy(&clearValues[2].color.float32, &clearColor, sizeof(clearValues[0].color.float32));
    vk::RenderPassBeginInfo renderPassInfo{s_Instance.m_OffscreenRenderPass.get(),
                                           s_Instance.m_OffscreenFramebuffer.get(),
                                           {{0, 0}, s_Instance.m_Extent},
                                           static_cast<uint32_t>(clearValues.size()),
                                           clearValues.data()};
    s_Instance.m_CommandBuffers[s_ImageIndex].get().beginRenderPass(renderPassInfo,
                                                                    vk::SubpassContents::eInline);
    s_Instance.m_CommandBuffers[s_ImageIndex].get().bindPipeline(
        vk::PipelineBindPoint::eGraphics, s_Instance.m_OffscreenGraphicsPipeline);
    s_Instance.m_CommandBuffers[s_ImageIndex].get().bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, s_Instance.m_OffscreenGraphicsPipeline.GetLayout(), 0, 1,
        &s_Instance.m_OffscreenDescriptorSets.Get()[s_ImageIndex], 0, nullptr);

    uint32_t instanceId = 0;
    for (auto &instance : instances)
    {
        PushConstant pushConstant{instanceId++, lightPosition, {1, 0, 1}};
        s_Instance.m_CommandBuffers[s_ImageIndex].get().pushConstants(
            s_Instance.m_OffscreenGraphicsPipeline.GetLayout(),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(PushConstant), &pushConstant);

        s_Instance.m_CommandBuffers[s_ImageIndex].get().bindVertexBuffers(
            0, {models[instance.objModelIndex].vertexBuffer.buffer}, {0});
        s_Instance.m_CommandBuffers[s_ImageIndex].get().bindIndexBuffer(
            models[instance.objModelIndex].indexBuffer.buffer, 0, vk::IndexType::eUint32);

        s_Instance.m_CommandBuffers[s_ImageIndex].get().drawIndexed(
            static_cast<uint32_t>(models[instance.objModelIndex].indicesCount), 1, 0, 0, 0);
    }

    s_Instance.m_CommandBuffers[s_ImageIndex].get().endRenderPass();
}

void VulkanRenderer::DrawImGui()
{
    vk::RenderPassBeginInfo renderPassInfo{s_Instance.m_ImGuiRenderPass.get(),
                                           s_Instance.m_ImGuiFrameBuffers[s_ImageIndex].get(),
                                           {{0, 0}, s_Instance.m_Extent}};
    s_Instance.m_CommandBuffers[s_ImageIndex].get().beginRenderPass(renderPassInfo,
                                                                    vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    s_Instance.m_CommandBuffers[s_ImageIndex].get());

    s_Instance.m_CommandBuffers[s_ImageIndex].get().endRenderPass();
}

void VulkanRenderer::PushModel(ObjModel model)
{
    models.push_back(model);
}

vk::CommandBuffer VulkanRenderer::BeginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{s_Instance.m_CommandPool.get(),
                                            vk::CommandBufferLevel::ePrimary, 1};
    vk::CommandBuffer commandBuffer =
        s_Instance.m_Device.get().allocateCommandBuffers(allocInfo)[0];
    vk::CommandBufferBeginInfo beginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffer.begin(beginInfo);
    return commandBuffer;
}

void VulkanRenderer::EndSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
    commandBuffer.end();
    vk::SubmitInfo submitInfo{0, nullptr, nullptr, 1, &commandBuffer};
    s_Instance.m_GraphicsQueue.submit(submitInfo, nullptr);
    s_Instance.m_GraphicsQueue.waitIdle();
    s_Instance.m_Device.get().freeCommandBuffers(s_Instance.m_CommandPool.get(), commandBuffer);
}

vk::ImageView VulkanRenderer::CreateImageView(vk::Image image, vk::Format format,
                                              vk::ImageAspectFlags aspectFlags)
{
    vk::ImageSubresourceRange subResourceRange(aspectFlags, 0, 1, 0, 1);
    vk::ImageViewCreateInfo imageViewCreateInfo{{},     image, vk::ImageViewType::e2D,
                                                format, {},    subResourceRange};
    return s_Instance.m_Device.get().createImageView(imageViewCreateInfo);
}

vk::UniqueImageView VulkanRenderer::CreateImageViewUnique(vk::Image image, vk::Format format,
                                                          vk::ImageAspectFlags aspectFlags)
{
    vk::ImageSubresourceRange subResourceRange(aspectFlags, 0, 1, 0, 1);
    vk::ImageViewCreateInfo imageViewCreateInfo{{},     image, vk::ImageViewType::e2D,
                                                format, {},    subResourceRange};
    return s_Instance.m_Device.get().createImageViewUnique(imageViewCreateInfo);
}

vk::Sampler VulkanRenderer::CreateSampler(const vk::SamplerCreateInfo createInfo)
{
    return s_Instance.m_Device.get().createSampler(createInfo);
}

void VulkanRenderer::InitRenderer(WindowsWindow *window)
{
    m_Window = window;
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateDevice();
    InitAllocator();
    CreateSwapChain();
    CreateSwapChainImageViews();
    CreateCommandPool();
    CreateOffscreenRenderer();
    CreatePostRenderer();
    CreateImGuiRenderer();
    CreateCommandBuffers();
    CreateSyncObjects();

    // Raytracing
    InitRayTracing();
}

void VulkanRenderer::CreateInstance()
{
    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceExtensions.insert(instanceExtensions.end(), glfwExtensions,
                              glfwExtensions + glfwExtensionCount);
    assert(CheckExtensionSupport(static_cast<uint32_t>(instanceExtensions.size()),
                                 instanceExtensions.data()));
    vk::ApplicationInfo applicationInfo("RTX ON", 1, "Vulkan engine", 1, VK_API_VERSION_1_0);
#ifdef NDEBUG
    vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, 0, nullptr,
                                              static_cast<uint32_t>(instanceExtensions.size()),
                                              instanceExtensions.data());
#else
    assert(CheckValidationLayerSupport());
    vk::InstanceCreateInfo instanceCreateInfo(
        {}, &applicationInfo, static_cast<uint32_t>(validationLayers.size()),
        validationLayers.data(), static_cast<uint32_t>(instanceExtensions.size()),
        instanceExtensions.data());
#endif
    m_Instance = vk::createInstanceUnique(instanceCreateInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance.get());
}

void VulkanRenderer::CreateSurface()
{
    VkSurfaceKHR surface;
    glfwCreateWindowSurface(m_Instance.get(), m_Window->GetNativeWindow(), nullptr, &surface);
    m_Surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(surface), m_Instance.get());
}

void VulkanRenderer::PickPhysicalDevice()
{
    std::vector<vk::PhysicalDevice> devices = m_Instance->enumeratePhysicalDevices();
    for (auto &device : devices)
    {
        if (IsDeviceSuitable(device, m_Surface.get()))
        {
            m_PhysicalDevice = device;
            return;
        }
    }
    throw std::runtime_error("Suitable physical device not found");
}

void VulkanRenderer::CreateDevice()
{
    m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice, m_Surface.get());

    std::set<uint32_t> queueFamilyIndices = {m_QueueFamilyIndices.graphicsFamily.value(),
                                             m_QueueFamilyIndices.presentFamily.value()};
    float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamilyIndex : queueFamilyIndices)
    {
        queueCreateInfos.push_back({{}, queueFamilyIndex, 1, &queuePriority});
    }
    vk::PhysicalDeviceScalarBlockLayoutFeatures scalarLayoutFeatures;
    scalarLayoutFeatures.scalarBlockLayout = VK_TRUE;
    vk::PhysicalDeviceDescriptorIndexingFeatures descriptorFeatures;
    descriptorFeatures.runtimeDescriptorArray = VK_TRUE;
    descriptorFeatures.pNext = &scalarLayoutFeatures;
    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    vk::PhysicalDeviceFeatures2 enabled;
    enabled.pNext = &descriptorFeatures;
    enabled.features = deviceFeatures;
    std::vector<vk::PhysicalDeviceFeatures> features = {deviceFeatures};
#ifdef NDEBUG
    vk::DeviceCreateInfo deviceCreateInfo{
        {},      static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data(), 0,
        nullptr, static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data(), nullptr};
#else
    vk::DeviceCreateInfo deviceCreateInfo{{},
                                          static_cast<uint32_t>(queueCreateInfos.size()),
                                          queueCreateInfos.data(),
                                          static_cast<uint32_t>(validationLayers.size()),
                                          validationLayers.data(),
                                          static_cast<uint32_t>(deviceExtensions.size()),
                                          deviceExtensions.data(),
                                          nullptr};
#endif
    deviceCreateInfo.pNext = &enabled;
    m_Device = m_PhysicalDevice.createDeviceUnique(deviceCreateInfo);

    m_GraphicsQueue = m_Device.get().getQueue(m_QueueFamilyIndices.graphicsFamily.value(), 0);
    m_PresentQueue = m_Device.get().getQueue(m_QueueFamilyIndices.presentFamily.value(), 0);

    vk::PhysicalDeviceProperties physicalDeviceProperties = m_PhysicalDevice.getProperties();
    size_t minUniformOffsetAlligment =
        physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    m_DynamicAlligment = sizeof(Material);
    if (minUniformOffsetAlligment > 0)
    {
        m_DynamicAlligment =
            (m_DynamicAlligment + minUniformOffsetAlligment - 1) & ~(minUniformOffsetAlligment - 1);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device.get());
}

void VulkanRenderer::InitAllocator()
{
    Allocator::Init(m_PhysicalDevice, m_Device.get());
}

void VulkanRenderer::CreateSwapChain()
{
    auto swapChainSupportDetails = QuerySwapChainSupport(m_PhysicalDevice, m_Surface.get());

    std::vector<vk::SurfaceFormatKHR> availableSurfaceFormats = swapChainSupportDetails.formats;
    assert(availableSurfaceFormats.size() > 0);
    vk::SurfaceFormatKHR surfaceFormat = availableSurfaceFormats[0];
    for (const auto &availableSurfaceFormat : availableSurfaceFormats)
    {
        if (availableSurfaceFormat.format == vk::Format::eB8G8R8A8Unorm &&
            availableSurfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            surfaceFormat = availableSurfaceFormat;
        }
    }
    m_SwapChainImageFormat = surfaceFormat.format;

    std::vector<vk::PresentModeKHR> availablePresentModes = swapChainSupportDetails.presentModes;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            presentMode = availablePresentMode;
        }
    }

    if (swapChainSupportDetails.capabilities.currentExtent.width != UINT32_MAX)
    {
        m_Extent = swapChainSupportDetails.capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(m_Window->GetNativeWindow(), &width, &height);
        m_Extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        m_Extent.width = std::max(
            swapChainSupportDetails.capabilities.minImageExtent.width,
            std::min(swapChainSupportDetails.capabilities.maxImageExtent.width, m_Extent.width));
        m_Extent.height = std::max(
            swapChainSupportDetails.capabilities.minImageExtent.height,
            std::min(swapChainSupportDetails.capabilities.maxImageExtent.height, m_Extent.height));
    }

    uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1;
    if (swapChainSupportDetails.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupportDetails.capabilities.maxImageCount)
    {
        imageCount = swapChainSupportDetails.capabilities.maxImageCount;
    }
    vk::SwapchainCreateInfoKHR swapChainCreateInfo(
        {}, m_Surface.get(), imageCount, m_SwapChainImageFormat, surfaceFormat.colorSpace, m_Extent,
        1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, 0, nullptr,
        swapChainSupportDetails.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, true, nullptr);

    if (m_QueueFamilyIndices.graphicsFamily != m_QueueFamilyIndices.presentFamily)
    {
        uint32_t queueFamilyIndices[] = {m_QueueFamilyIndices.graphicsFamily.value(),
                                         m_QueueFamilyIndices.presentFamily.value()};
        swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    m_SwapChain.reset();
    m_SwapChain = m_Device.get().createSwapchainKHRUnique(swapChainCreateInfo);
    m_SwapChainImages = m_Device.get().getSwapchainImagesKHR(m_SwapChain.get());
}

void VulkanRenderer::RecreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window->GetNativeWindow(), &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window->GetNativeWindow(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_Device.get());

    CreateSwapChain();
    CreateSwapChainImageViews();
    CreateOffscreenRenderer();
    CreatePostRenderer();
    CreateImGuiRenderer();
    CreateCommandBuffers();
    Flush(instances);
}

void VulkanRenderer::IntegrateImGui()
{
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eSampler, 1000},
        {vk::DescriptorType::eCombinedImageSampler, 1000},
        {vk::DescriptorType::eSampledImage, 1000},
        {vk::DescriptorType::eStorageImage, 1000},
        {vk::DescriptorType::eUniformTexelBuffer, 1000},
        {vk::DescriptorType::eStorageTexelBuffer, 1000},
        {vk::DescriptorType::eUniformBuffer, 1000},
        {vk::DescriptorType::eStorageBuffer, 1000},
        {vk::DescriptorType::eUniformBufferDynamic, 1000},
        {vk::DescriptorType::eStorageBufferDynamic, 1000},
        {vk::DescriptorType::eInputAttachment, 1000}};

    m_ImGuiDescriptorPool = vk::UniqueDescriptorPool(
        util::CreateDescriptorPool(m_Device.get(), poolSizes,
                                   1000 * static_cast<uint32_t>(poolSizes.size())),
        m_Device.get());

    ImGui_ImplVulkan_InitInfo imguiInfo = {};
    imguiInfo.Instance = m_Instance.get();
    imguiInfo.PhysicalDevice = m_PhysicalDevice;
    imguiInfo.Device = m_Device.get();
    imguiInfo.QueueFamily = m_QueueFamilyIndices.graphicsFamily.value();
    imguiInfo.Queue = m_GraphicsQueue;
    imguiInfo.PipelineCache = nullptr;
    imguiInfo.DescriptorPool = m_ImGuiDescriptorPool.get();
    imguiInfo.Allocator = nullptr;
    imguiInfo.MinImageCount = (uint32_t)m_SwapChainImages.size();
    imguiInfo.ImageCount = (uint32_t)m_SwapChainImages.size();
    imguiInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&imguiInfo, m_ImGuiRenderPass.get());

    vk::CommandBuffer commandBuffer = BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    EndSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::CreateSwapChainImageViews()
{
    m_SwapChainImageViews.resize(m_SwapChainImages.size());
    for (uint32_t i = 0; i < m_SwapChainImages.size(); i++)
    {
        m_SwapChainImageViews[i] = CreateImageViewUnique(
            m_SwapChainImages[i], m_SwapChainImageFormat, vk::ImageAspectFlagBits::eColor);
    }
}

void VulkanRenderer::CreateOffscreenRenderer()
{
    m_SampledImage.textureAllocation = Allocator::CreateImage(
        m_Extent.width, m_Extent.height, msaaSamples, vk::Format::eR32G32B32A32Sfloat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
        VMA_MEMORY_USAGE_GPU_ONLY);
    Allocator::TransitionImageLayout(m_SampledImage.textureAllocation.image,
                                     vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eGeneral);
    m_SampledImage.descriptor.imageView =
        CreateImageView(m_SampledImage.textureAllocation.image, vk::Format::eR32G32B32A32Sfloat,
                        vk::ImageAspectFlagBits::eColor);

    m_OffscreenImageAllocation.textureAllocation = Allocator::CreateImage(
        m_Extent.width, m_Extent.height, vk::SampleCountFlagBits::e1,
        vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_GPU_ONLY);
    Allocator::TransitionImageLayout(m_OffscreenImageAllocation.textureAllocation.image,
                                     vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eGeneral);
    m_OffscreenImageAllocation.descriptor.imageView =
        CreateImageView(m_OffscreenImageAllocation.textureAllocation.image,
                        vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
    m_OffscreenImageAllocation.descriptor.sampler = CreateSampler(vk::SamplerCreateInfo());
    m_OffscreenImageAllocation.descriptor.imageLayout = vk::ImageLayout::eGeneral;

    m_OffscreenDepthImageAllocation.textureAllocation = Allocator::CreateImage(
        m_Extent.width, m_Extent.height, msaaSamples, vk::Format::eD32Sfloat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_GPU_ONLY);
    Allocator::TransitionImageLayout(m_OffscreenDepthImageAllocation.textureAllocation.image,
                                     vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eDepthStencilAttachmentOptimal);
    m_OffscreenDepthImageAllocation.descriptor.imageView =
        CreateImageView(m_OffscreenDepthImageAllocation.textureAllocation.image,
                        vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth);
    m_OffscreenRenderPass = vk::UniqueRenderPass(
        util::CreateRenderPass(m_Device.get(), vk::Format::eR32G32B32A32Sfloat, msaaSamples, true,
                               vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                               vk::Format::eD32Sfloat, true,
                               vk::ImageLayout::eDepthStencilAttachmentOptimal,
                               vk::ImageLayout::eDepthStencilAttachmentOptimal, true),
        m_Device.get());

    std::vector<vk::ImageView> attachments = {m_SampledImage.descriptor.imageView,
                                              m_OffscreenDepthImageAllocation.descriptor.imageView,
                                              m_OffscreenImageAllocation.descriptor.imageView};

    vk::FramebufferCreateInfo info;
    info.setRenderPass(m_OffscreenRenderPass.get());
    info.setAttachmentCount(static_cast<uint32_t>(attachments.size()));
    info.setPAttachments(attachments.data());
    info.setWidth(m_Extent.width);
    info.setHeight(m_Extent.height);
    info.setLayers(1);
    m_OffscreenFramebuffer = m_Device.get().createFramebufferUnique(info);
}

void VulkanRenderer::CreatePostRenderer()
{
    m_PostRenderPass = vk::UniqueRenderPass(
        util::CreateRenderPass(m_Device.get(), m_SwapChainImageFormat, vk::SampleCountFlagBits::e1,
                               true, vk::ImageLayout::eUndefined,
                               vk::ImageLayout::eColorAttachmentOptimal,
                               FindDepthFormat(m_PhysicalDevice), true, vk::ImageLayout::eUndefined,
                               vk::ImageLayout::eDepthStencilAttachmentOptimal),
        m_Device.get());

    vk::Format depthFormat = FindDepthFormat(m_PhysicalDevice);
    m_DepthImageAllocation = Allocator::CreateImage(
        m_Extent.width, m_Extent.height, vk::SampleCountFlagBits::e1, depthFormat,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_DepthImageView = CreateImageViewUnique(m_DepthImageAllocation.image, depthFormat,
                                             vk::ImageAspectFlagBits::eDepth);

    m_PostFrameBuffers.resize(m_SwapChainImageViews.size());
    for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
    {
        std::vector<vk::ImageView> attachments = {m_SwapChainImageViews[i].get(),
                                                  m_DepthImageView.get()};
        vk::FramebufferCreateInfo framebufferInfo{{},
                                                  m_PostRenderPass.get(),
                                                  static_cast<uint32_t>(attachments.size()),
                                                  attachments.data(),
                                                  m_Extent.width,
                                                  m_Extent.height,
                                                  1};
        m_PostFrameBuffers[i] = m_Device.get().createFramebufferUnique(framebufferInfo);
    }
}

void VulkanRenderer::CreateImGuiRenderer()
{
    m_ImGuiRenderPass = vk::UniqueRenderPass(
        util::CreateRenderPass(m_Device.get(), m_SwapChainImageFormat, vk::SampleCountFlagBits::e1,
                               false, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR),
        m_Device.get());

    m_ImGuiFrameBuffers.resize(m_SwapChainImageViews.size());
    for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
    {
        std::array<vk::ImageView, 1> attachments = {m_SwapChainImageViews[i].get()};
        vk::FramebufferCreateInfo framebufferInfo{{},
                                                  m_ImGuiRenderPass.get(),
                                                  static_cast<uint32_t>(attachments.size()),
                                                  attachments.data(),
                                                  m_Extent.width,
                                                  m_Extent.height,
                                                  1};
        m_ImGuiFrameBuffers[i] = m_Device.get().createFramebufferUnique(framebufferInfo);
    }
}

void VulkanRenderer::CreateOffscreenDescriptorResources()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.push_back({0, vk::DescriptorType::eUniformBuffer, 1,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eRaygenNV});
    bindings.push_back(
        {1, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(models.size()),
         vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eClosestHitNV});
    bindings.push_back(
        {2, vk::DescriptorType::eCombinedImageSampler,
         static_cast<uint32_t>(ObjModel::s_TextureImages.size()),
         vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eClosestHitNV});
    bindings.push_back({3, vk::DescriptorType::eStorageBuffer, 1,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                            vk::ShaderStageFlagBits::eClosestHitNV});
    bindings.push_back({4, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(models.size()),
                        vk::ShaderStageFlagBits::eClosestHitNV});
    bindings.push_back({5, vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(models.size()),
                        vk::ShaderStageFlagBits::eClosestHitNV});

    m_OffscreenDescriptorSets.Init(m_Device.get());
    m_OffscreenDescriptorSets.CreateDescriptorSetLayout(bindings);
    m_OffscreenDescriptorPool = vk::UniqueDescriptorPool(
        util::CreateDescriptorPool(m_Device.get(), bindings,
                                   static_cast<uint32_t>(m_SwapChainImages.size())),
        m_Device.get());
    m_OffscreenDescriptorSets.CreateDescriptorSets(m_OffscreenDescriptorPool.get(),
                                                   static_cast<uint32_t>(m_SwapChainImages.size()));
}

void VulkanRenderer::CreatePostDescriptorResources()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.push_back(
        {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment});
    m_PostDescriptorSets.Init(m_Device.get());
    m_PostDescriptorSets.CreateDescriptorSetLayout(bindings);
    m_PostDescriptorPool = vk::UniqueDescriptorPool(
        util::CreateDescriptorPool(m_Device.get(), bindings, 1), m_Device.get());
    m_PostDescriptorSets.CreateDescriptorSets(m_PostDescriptorPool.get(), 1);
}

void VulkanRenderer::UpdateOffscreenDescriptorSets()
{
    vk::DescriptorBufferInfo cameraBufferInfo{m_CameraBufferAllocations[0].buffer, 0,
                                              sizeof(CameraMatrices)};
    vk::DescriptorBufferInfo instanceBufferInfo{instanceBufferAlloc.buffer, 0, VK_WHOLE_SIZE};
    std::vector<vk::DescriptorBufferInfo> materialBufferInfo;
    std::vector<vk::DescriptorBufferInfo> vertexBufferInfo;
    std::vector<vk::DescriptorBufferInfo> indexBufferInfo;
    for (size_t i = 0; i < models.size(); ++i)
    {
        materialBufferInfo.push_back({models[i].materialBuffer.buffer, 0, VK_WHOLE_SIZE});
        vertexBufferInfo.push_back({models[i].vertexBuffer.buffer, 0, VK_WHOLE_SIZE});
        indexBufferInfo.push_back({models[i].indexBuffer.buffer, 0, VK_WHOLE_SIZE});
    }

    std::vector<vk::DescriptorImageInfo> texturesBufferInfo;
    for (size_t i = 0; i < ObjModel::s_TextureImages.size(); ++i)
    {
        texturesBufferInfo.push_back(ObjModel::s_TextureImages[i].descriptor);
    }

    std::vector<vk::WriteDescriptorSet> descriptorWrites = {
        m_OffscreenDescriptorSets.CreateWrite(0, &cameraBufferInfo, 0),
        m_OffscreenDescriptorSets.CreateWrite(1, materialBufferInfo.data(), 0),
        m_OffscreenDescriptorSets.CreateWrite(2, texturesBufferInfo.data(), 0),
        m_OffscreenDescriptorSets.CreateWrite(3, &instanceBufferInfo, 0),
        m_OffscreenDescriptorSets.CreateWrite(4, vertexBufferInfo.data(), 0),
        m_OffscreenDescriptorSets.CreateWrite(5, indexBufferInfo.data(), 0)};

    m_OffscreenDescriptorSets.Update(descriptorWrites);
}

void VulkanRenderer::UpdatePostDescriptorSets()
{
    std::vector<vk::WriteDescriptorSet> descriptorWrites = {
        m_PostDescriptorSets.CreateWrite(0, &m_OffscreenImageAllocation.descriptor, 0)};
    m_PostDescriptorSets.Update(descriptorWrites);
}

void VulkanRenderer::CreateOffscreenGraphicsPipeline()
{
    m_OffscreenGraphicsPipeline.Init(m_Device.get());
    m_OffscreenGraphicsPipeline.LoadVertexShader("src/Shaders/vert.spv");
    m_OffscreenGraphicsPipeline.LoadFragmentShader("src/Shaders/frag.spv");

    vk::PushConstantRange pushConstantRange = {vk::ShaderStageFlagBits::eVertex |
                                                   vk::ShaderStageFlagBits::eFragment,
                                               0, sizeof(PushConstant)};
    m_OffscreenGraphicsPipeline.CreatePipelineLayout({m_OffscreenDescriptorSets.GetLayout()},
                                                     {pushConstantRange});
    m_OffscreenGraphicsPipeline.CreatePipeline(
        m_OffscreenRenderPass.get(), msaaSamples, m_Extent, {Vertex::getBindingDescription()},
        {Vertex::getAttributeDescriptions()}, vk::CullModeFlagBits::eBack);
}

void VulkanRenderer::CreatePostGraphicsPipeline()
{
    m_PostGraphicsPipeline.Init(m_Device.get());
    m_PostGraphicsPipeline.LoadVertexShader("src/Shaders/vert_post.spv");
    m_PostGraphicsPipeline.LoadFragmentShader("src/Shaders/frag_post.spv");

    vk::PushConstantRange pushConstantRange = {vk::ShaderStageFlagBits::eFragment, 0,
                                               sizeof(float)};
    m_PostGraphicsPipeline.CreatePipelineLayout({m_PostDescriptorSets.GetLayout()},
                                                {pushConstantRange});
    m_PostGraphicsPipeline.CreatePipeline(m_PostRenderPass.get(), vk::SampleCountFlagBits::e1,
                                          m_Extent, {}, {}, vk::CullModeFlagBits::eNone);
}

void VulkanRenderer::CreateCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       m_QueueFamilyIndices.graphicsFamily.value()};
    m_CommandPool = m_Device.get().createCommandPoolUnique(poolInfo);
}

void VulkanRenderer::CreateUniformBuffers(std::vector<BufferAllocation> &bufferAllocations,
                                          vk::DeviceSize bufferSize)
{
    bufferAllocations.resize(m_SwapChainImages.size());
    for (size_t i = 0; i < m_SwapChainImages.size(); i++)
    {
        bufferAllocations[i] = Allocator::CreateBuffer(
            bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
}

void VulkanRenderer::CreateCommandBuffers()
{
    m_CommandBuffers.resize(m_PostFrameBuffers.size());
    vk::CommandBufferAllocateInfo allocInfo{m_CommandPool.get(), vk::CommandBufferLevel::ePrimary,
                                            static_cast<uint32_t>(m_CommandBuffers.size())};
    m_CommandBuffers = m_Device.get().allocateCommandBuffersUnique(allocInfo);
}

void VulkanRenderer::CreateSyncObjects()
{
    m_ImagesInFlight.resize(m_SwapChainImages.size());

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{vk::FenceCreateFlagBits::eSignaled};

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_ImageAvailableSemaphores.push_back(m_Device.get().createSemaphoreUnique(semaphoreInfo));
        m_RenderFinishedSemaphores.push_back(m_Device.get().createSemaphoreUnique(semaphoreInfo));
        m_InFlightFences.push_back(m_Device.get().createFenceUnique(fenceInfo));
    }
}

void VulkanRenderer::UpdateCameraMatrices(const PerspectiveCamera &camera)
{
    auto view = camera.GetViewMatrix();
    auto projection = camera.GetProjectionMatrix();
    CameraMatrices m = {camera.GetPosition(), view, projection, glm::inverse(view),
                        glm::inverse(projection)};
    Allocator::UpdateAllocation(m_CameraBufferAllocations[s_ImageIndex].allocation, m);
}

void VulkanRenderer::Raytrace(const glm::vec4 &clearColor, const glm::vec3 &lightPosition,
                              uint32_t nSamples)
{
    RtPushConstant rtPushConstant;
    rtPushConstant.clearColor = clearColor;
    rtPushConstant.lightPosition = lightPosition;
    rtPushConstant.lightIntensity = lightIntensity;
    rtPushConstant.lightType = lightType;
    rtPushConstant.nSamples = nSamples;
    rtPushConstant.hdr = hdr;
    rtPushConstant.ni = ni;
    rtPushConstant.F0 = F0;

    auto &cmdBuf = s_Instance.m_CommandBuffers[s_ImageIndex].get();

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, s_Instance.m_RtPipeline.get());
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eRayTracingNV, s_Instance.m_RtPipelineLayout.get(), 0,
        {s_Instance.m_RtDescriptorSet.Get()[0], s_Instance.m_OffscreenDescriptorSets.Get()[0]}, {});
    cmdBuf.pushConstants<RtPushConstant>(s_Instance.m_RtPipelineLayout.get(),
                                         vk::ShaderStageFlagBits::eRaygenNV |
                                             vk::ShaderStageFlagBits::eClosestHitNV |
                                             vk::ShaderStageFlagBits::eMissNV,
                                         0, rtPushConstant);

    vk::DeviceSize progSize = s_Instance.m_RtProperties.shaderGroupHandleSize;
    vk::DeviceSize rayGenOffset = 0u * progSize;
    vk::DeviceSize missOffset = 1u * progSize;
    vk::DeviceSize missStride = progSize;
    vk::DeviceSize hitGroupOffset = 3u * progSize;
    vk::DeviceSize hitGroupStride = progSize;

    cmdBuf.traceRaysNV(s_Instance.m_RtSBTBufferAllocation.buffer, rayGenOffset,
                       s_Instance.m_RtSBTBufferAllocation.buffer, missOffset, missStride,
                       s_Instance.m_RtSBTBufferAllocation.buffer, hitGroupOffset, hitGroupStride,
                       s_Instance.m_RtSBTBufferAllocation.buffer, 0, 0, s_Instance.m_Extent.width,
                       s_Instance.m_Extent.height, 1);
}

void VulkanRenderer::InitRayTracing()
{
    auto properties = m_PhysicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
                                                      vk::PhysicalDeviceRayTracingPropertiesNV>();
    m_RtProperties = properties.get<vk::PhysicalDeviceRayTracingPropertiesNV>();
}

void VulkanRenderer::CreateBottomLevelAS()
{
    std::vector<std::vector<vk::GeometryNV>> geometry;
    geometry.reserve(models.size());
    for (size_t i = 0; i < models.size(); i++)
    {
        auto geo = objectToVkGeometryNV(models[i]);
        geometry.push_back({geo});
    }

    m_Blas.resize(geometry.size());
    vk::DeviceSize maxScratch{0};
    for (size_t i = 0; i < geometry.size(); i++)
    {
        Blas &blas{m_Blas[i]};
        blas.asInfo.setGeometryCount(static_cast<uint32_t>(geometry[i].size()));
        blas.asInfo.setPGeometries(geometry[i].data());
        blas.asInfo.flags = vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace;
        vk::AccelerationStructureCreateInfoNV createInfo{0, blas.asInfo};
        blas.as = Allocator::CreateAcceleration(createInfo);

        vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{
            vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch, blas.as.accel};
        vk::DeviceSize scratchSize =
            m_Device.get()
                .getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo)
                .memoryRequirements.size;

        maxScratch = std::max(maxScratch, scratchSize);
    }

    BufferAllocation scratchBuffer = Allocator::CreateBuffer(
        maxScratch, vk::BufferUsageFlagBits::eRayTracingNV, VMA_MEMORY_USAGE_GPU_ONLY);

    vk::CommandBuffer cmdBuf = BeginSingleTimeCommands();
    for (auto &blas : m_Blas)
    {
        cmdBuf.buildAccelerationStructureNV(blas.asInfo, nullptr, 0, VK_FALSE, blas.as.accel,
                                            nullptr, scratchBuffer.buffer, 0);
        vk::MemoryBarrier barrier(vk::AccessFlagBits::eAccelerationStructureWriteNV,
                                  vk::AccessFlagBits::eAccelerationStructureReadNV);
        cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
                               vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
                               vk::DependencyFlags(), {barrier}, {}, {});
    }
    EndSingleTimeCommands(cmdBuf);

    // TODO: dstroy scratch buffer
}

void VulkanRenderer::CreateTopLevelAS(const std::vector<ObjInstance> &objInstances)
{
    std::vector<BlasInstance> tlas;
    tlas.reserve(objInstances.size());
    for (int i = 0; i < static_cast<int>(objInstances.size()); i++)
    {
        BlasInstance rayInst;
        rayInst.transform = objInstances[i].modelMatrix;
        rayInst.instanceId = i;
        rayInst.blasId = objInstances[i].objModelIndex;
        rayInst.hitGroupId = 0;
        rayInst.flags = vk::GeometryInstanceFlagBitsNV::eTriangleCullDisable;
        tlas.emplace_back(rayInst);
    }

    m_Tlas.asInfo.instanceCount = static_cast<uint32_t>(objInstances.size());
    m_Tlas.asInfo.flags = vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace;
    vk::AccelerationStructureCreateInfoNV accelerationStructureInfo{0, m_Tlas.asInfo};
    m_Tlas.as = Allocator::CreateAcceleration(accelerationStructureInfo);

    vk::AccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{
        vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch, m_Tlas.as.accel};
    vk::DeviceSize scratchSize =
        m_Device.get()
            .getAccelerationStructureMemoryRequirementsNV(memoryRequirementsInfo)
            .memoryRequirements.size;

    BufferAllocation scratchBuffer = Allocator::CreateBuffer(
        scratchSize, vk::BufferUsageFlagBits::eRayTracingNV, VMA_MEMORY_USAGE_GPU_ONLY);

    std::vector<VkGeometryInstanceNV> geometryInstances;
    geometryInstances.reserve(tlas.size());
    for (const auto &inst : tlas)
    {
        geometryInstances.push_back(InstanceToVkGeometryInstanceNV(inst));
    }

    vk::CommandBuffer cmdBuf = BeginSingleTimeCommands();
    vk::DeviceSize instanceDescsSizeInBytes = tlas.size() * sizeof(VkGeometryInstanceNV);
    m_TlasAlloc = Allocator::CreateDeviceLocalBuffer(cmdBuf, geometryInstances,
                                                     vk::BufferUsageFlagBits::eRayTracingNV);
    vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite,
                              vk::AccessFlagBits::eAccelerationStructureWriteNV);
    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
                           vk::DependencyFlags(), {barrier}, {}, {});
    cmdBuf.buildAccelerationStructureNV(m_Tlas.asInfo, m_TlasAlloc.buffer, 0, VK_FALSE,
                                        m_Tlas.as.accel, nullptr, scratchBuffer.buffer, 0);
    EndSingleTimeCommands(cmdBuf);

    Allocator::FlushStaging();

    // TODO: Destroy scratch buffer
}

void VulkanRenderer::CreateRtDescriptorResources()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eAccelerationStructureNV, 1,
                                       vk::ShaderStageFlagBits::eRaygenNV |
                                           vk::ShaderStageFlagBits::eClosestHitNV),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1,
                                       vk::ShaderStageFlagBits::eRaygenNV),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1,
                                       vk::ShaderStageFlagBits::eMissNV),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1,
                                       vk::ShaderStageFlagBits::eMissNV)};

    m_RtDescriptorSet.Init(m_Device.get());
    m_RtDescriptorSet.CreateDescriptorSetLayout(bindings);
    m_RtDescriptorPool = vk::UniqueDescriptorPool(
        util::CreateDescriptorPool(m_Device.get(), bindings, 1), m_Device.get());
    m_RtDescriptorSet.CreateDescriptorSets(m_RtDescriptorPool.get(), 1);
}

void VulkanRenderer::UpdateRtDescriptorSets()
{
    vk::WriteDescriptorSetAccelerationStructureNV descASInfo;
    descASInfo.setAccelerationStructureCount(1);
    descASInfo.setPAccelerationStructures(&m_Tlas.as.accel);

    vk::DescriptorImageInfo imageInfo{
        {}, m_OffscreenImageAllocation.descriptor.imageView, vk::ImageLayout::eGeneral};

    std::vector<vk::WriteDescriptorSet> descriptorWrites = {
        m_RtDescriptorSet.CreateWrite(0, &descASInfo, 0),
        m_RtDescriptorSet.CreateWrite(1, &imageInfo, 0),
        m_RtDescriptorSet.CreateWrite(2, &ObjModel::s_Skysphere.descriptor, 0),
        m_RtDescriptorSet.CreateWrite(3, &ObjModel::s_HdrSkysphere.descriptor, 0)};
    m_RtDescriptorSet.Update(descriptorWrites);
}

void VulkanRenderer::CreateRtPipeline()
{
    VulkanShader rayGen(m_Device.get());
    rayGen.LoadFromFile("src/Shaders/raytrace_rgen.spv", vk::ShaderStageFlagBits::eRaygenNV);
    VulkanShader rayMiss(m_Device.get());
    rayMiss.LoadFromFile("src/Shaders/raytrace_rmiss.spv", vk::ShaderStageFlagBits::eMissNV);
    VulkanShader rayHit(m_Device.get());
    rayHit.LoadFromFile("src/Shaders/raytrace_rchit.spv", vk::ShaderStageFlagBits::eClosestHitNV);
    VulkanShader rayShadowMiss(m_Device.get());
    rayShadowMiss.LoadFromFile("src/Shaders/raytrace_shadow_rmiss.spv",
                               vk::ShaderStageFlagBits::eMissNV);

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    // Raygen
    vk::RayTracingShaderGroupCreateInfoNV rg{vk::RayTracingShaderGroupTypeNV::eGeneral,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV};
    shaderStages.push_back(rayGen.GetShaderStageCreateInfo());
    rg.setGeneralShader(static_cast<uint32_t>(shaderStages.size() - 1));
    m_RtShaderGroups.push_back(rg);

    // Miss
    vk::RayTracingShaderGroupCreateInfoNV mg{vk::RayTracingShaderGroupTypeNV::eGeneral,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV};
    shaderStages.push_back(rayMiss.GetShaderStageCreateInfo());
    mg.setGeneralShader(static_cast<uint32_t>(shaderStages.size() - 1));
    m_RtShaderGroups.push_back(mg);

    // Shadow Miss
    shaderStages.push_back(rayShadowMiss.GetShaderStageCreateInfo());
    mg.setGeneralShader(static_cast<uint32_t>(shaderStages.size() - 1));
    m_RtShaderGroups.push_back(mg);

    // Hit Group - Closest Hit + AnyHit
    vk::RayTracingShaderGroupCreateInfoNV hg{vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV,
                                             VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV};
    shaderStages.push_back(rayHit.GetShaderStageCreateInfo());
    hg.setClosestHitShader(static_cast<uint32_t>(shaderStages.size() - 1));
    m_RtShaderGroups.push_back(hg);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    // Push constant: we want to be able to update constants used by the shaders
    vk::PushConstantRange pushConstant{vk::ShaderStageFlagBits::eRaygenNV |
                                           vk::ShaderStageFlagBits::eClosestHitNV |
                                           vk::ShaderStageFlagBits::eMissNV,
                                       0, sizeof(RtPushConstant)};
    pipelineLayoutCreateInfo.setPushConstantRangeCount(1);
    pipelineLayoutCreateInfo.setPPushConstantRanges(&pushConstant);

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
    std::vector<vk::DescriptorSetLayout> rtDescSetLayouts = {m_RtDescriptorSet.GetLayout(),
                                                             m_OffscreenDescriptorSets.GetLayout()};
    pipelineLayoutCreateInfo.setSetLayoutCount(static_cast<uint32_t>(rtDescSetLayouts.size()));
    pipelineLayoutCreateInfo.setPSetLayouts(rtDescSetLayouts.data());

    m_RtPipelineLayout = m_Device.get().createPipelineLayoutUnique(pipelineLayoutCreateInfo);

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    vk::RayTracingPipelineCreateInfoNV rayPipelineInfo;
    rayPipelineInfo.setStageCount(static_cast<uint32_t>(shaderStages.size()));
    rayPipelineInfo.setPStages(shaderStages.data());

    rayPipelineInfo.setGroupCount(static_cast<uint32_t>(m_RtShaderGroups.size()));
    rayPipelineInfo.setPGroups(m_RtShaderGroups.data());

    rayPipelineInfo.setMaxRecursionDepth(10);
    rayPipelineInfo.setLayout(m_RtPipelineLayout.get());
    m_RtPipeline = m_Device.get().createRayTracingPipelineNVUnique({}, rayPipelineInfo);
    assert(m_RtPipeline);
}

void VulkanRenderer::CreateRtShaderBindingTable()
{
    auto groupCount = static_cast<uint32_t>(m_RtShaderGroups.size());
    uint32_t groupHandleSize = m_RtProperties.shaderGroupHandleSize;

    uint32_t sbtSize = groupCount * groupHandleSize;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    m_Device.get().getRayTracingShaderGroupHandlesNV(m_RtPipeline.get(), 0, groupCount, sbtSize,
                                                     shaderHandleStorage.data());
    // Write the handles in the SBT
    auto cmdBuf = BeginSingleTimeCommands();
    m_RtSBTBufferAllocation = Allocator::CreateDeviceLocalBuffer(
        cmdBuf, shaderHandleStorage, vk::BufferUsageFlagBits::eRayTracingNV);
    EndSingleTimeCommands(cmdBuf);

    Allocator::FlushStaging();
}

vk::GeometryNV VulkanRenderer::objectToVkGeometryNV(const ObjModel &model)
{
    vk::GeometryTrianglesNV triangles;
    triangles.setVertexData(model.vertexBuffer.buffer);
    triangles.setVertexOffset(0);
    triangles.setVertexCount(model.verticesCount);
    triangles.setVertexStride(sizeof(Vertex));
    triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
    triangles.setIndexData(model.indexBuffer.buffer);
    triangles.setIndexOffset(0);
    triangles.setIndexCount(model.indicesCount);
    triangles.setIndexType(vk::IndexType::eUint32);
    vk::GeometryDataNV geoData;
    geoData.setTriangles(triangles);
    vk::GeometryNV geometry;
    geometry.setGeometry(geoData);
    geometry.setFlags(vk::GeometryFlagBitsNV::eOpaque);
    return geometry;
}

VkGeometryInstanceNV VulkanRenderer::InstanceToVkGeometryInstanceNV(const BlasInstance &instance)
{
    Blas &blas{m_Blas[instance.blasId]};
    uint64_t asHandle = 0;
    m_Device.get().getAccelerationStructureHandleNV(blas.as.accel, sizeof(uint64_t), &asHandle);
    VkGeometryInstanceNV gInst{};
    glm::mat4 transp = glm::transpose(instance.transform);
    memcpy(gInst.transform, &transp, sizeof(gInst.transform));
    gInst.instanceId = instance.instanceId;
    gInst.mask = instance.mask;
    gInst.hitGroupId = instance.hitGroupId;
    gInst.flags = static_cast<uint32_t>(instance.flags);
    gInst.accelerationStructureHandle = asHandle;
    return gInst;
}
