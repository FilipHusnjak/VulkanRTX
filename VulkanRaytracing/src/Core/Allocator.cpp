#include "vkpch.h"

#define VMA_IMPLEMENTATION
#include "allocator.h"

#include "Renderer/VulkanRenderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Allocator Allocator::s_Allocator;

void Allocator::Init(vk::PhysicalDevice physicalDevice, vk::Device device)
{
    s_Allocator.m_PhysicalDevice = physicalDevice;
    s_Allocator.m_Device = device;
    VmaAllocatorCreateInfo allocatorInfo{{}, physicalDevice, device};
    allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    vmaCreateAllocator(&allocatorInfo, &s_Allocator.m_Allocator);
}

void Allocator::FlushStaging()
{
    while (!s_Allocator.m_StagingBuffers.empty())
    {
        BufferAllocation &stagingBufferAllocation = s_Allocator.m_StagingBuffers.front();
        s_Allocator.m_StagingBuffers.pop();
        vmaDestroyBuffer(s_Allocator.m_Allocator, stagingBufferAllocation.buffer,
                         stagingBufferAllocation.allocation);
    }
}

BufferAllocation Allocator::CreateBuffer(const vk::DeviceSize &size,
                                         const vk::BufferUsageFlags &usage,
                                         const VmaMemoryUsage &memoryUsage)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    BufferAllocation bufferAllocation{};
    vmaCreateBuffer(s_Allocator.m_Allocator, &bufferInfo, &allocInfo, &bufferAllocation.buffer,
                    &bufferAllocation.allocation, nullptr);
    return bufferAllocation;
}

ImageAllocation Allocator::CreateImage(const uint32_t width, const uint32_t height,
                                       vk::SampleCountFlagBits sampleCount,
                                       const vk::Format &format, const vk::ImageTiling &tiling,
                                       const vk::ImageUsageFlags &usage,
                                       const VmaMemoryUsage &memoryUsage)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
    imageInfo.tiling = static_cast<VkImageTiling>(tiling);
    imageInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageAllocation imageAllocation{};
    vmaCreateImage(s_Allocator.m_Allocator, &imageInfo, &allocInfo, &imageAllocation.image,
                   &imageAllocation.allocation, nullptr);
    return imageAllocation;
}

void Allocator::TransitionImageLayout(vk::Image image, vk::ImageAspectFlagBits aspect,
                                      vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::ImageSubresourceRange imgSubresourceRange{aspect, 0, 1, 0, 1};
    vk::ImageMemoryBarrier barrier{{},
                                   {},
                                   oldLayout,
                                   newLayout,
                                   VK_QUEUE_FAMILY_IGNORED,
                                   VK_QUEUE_FAMILY_IGNORED,
                                   image,
                                   imgSubresourceRange};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;
    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eGeneral)
    {
        barrier.srcAccessMask = vk::AccessFlagBits();
        barrier.dstAccessMask = vk::AccessFlagBits();

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
    }
    else if (oldLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits();
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }
    else
    {
        assert(false);
    }
    auto commandBuffer = VulkanRenderer::BeginSingleTimeCommands();
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, {barrier});
    VulkanRenderer::EndSingleTimeCommands(commandBuffer);
}

ImageAllocation Allocator::CreateTextureImage(const std::string &filename)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels =
        stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        texWidth = texHeight = 1;
        texChannels = 4;
        glm::u8vec4 *color = new glm::u8vec4(255, 128, 0, 128);
        pixels = reinterpret_cast<stbi_uc *>(color);
    }

    vk::DeviceSize imageSize =
        static_cast<uint64_t>(texWidth) * static_cast<uint64_t>(texHeight) * sizeof(glm::u8vec4);

    BufferAllocation stagingBufferAllocation =
        CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_GPU_TO_CPU);

    void *mappedData;
    vmaMapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation, &mappedData);
    memcpy(mappedData, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation);

    stbi_image_free(pixels);

    ImageAllocation imageAllocation =
        CreateImage(texWidth, texHeight, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    TransitionImageLayout(imageAllocation.image, vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::ImageSubresourceLayers imgSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    vk::BufferImageCopy region{
        0,
        0,
        0,
        imgSubresourceLayers,
        {0, 0, 0},
        vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1}};

    auto commandBuffer = VulkanRenderer::BeginSingleTimeCommands();
    commandBuffer.copyBufferToImage(stagingBufferAllocation.buffer, imageAllocation.image,
                                    vk::ImageLayout::eTransferDstOptimal, {region});
    VulkanRenderer::EndSingleTimeCommands(commandBuffer);

    TransitionImageLayout(imageAllocation.image, vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    s_Allocator.m_StagingBuffers.push(stagingBufferAllocation);
    return imageAllocation;
}

ImageAllocation Allocator::CreateHdrTextureImage(const std::string &filename)
{
    int texWidth, texHeight, nrComponents;
    float *pixels =
        stbi_loadf(filename.c_str(), &texWidth, &texHeight, &nrComponents, 0);

    vk::DeviceSize imageSize =
        static_cast<uint64_t>(texWidth) * static_cast<uint64_t>(texHeight) * sizeof(float) * 3;

    BufferAllocation stagingBufferAllocation =
        CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_GPU_TO_CPU);

    void *mappedData;
    vmaMapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation, &mappedData);
    memcpy(mappedData, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation);

    stbi_image_free(pixels);

    ImageAllocation imageAllocation =
        CreateImage(texWidth, texHeight, vk::SampleCountFlagBits::e1, vk::Format::eR32G32B32Sfloat,
                    vk::ImageTiling::eLinear,
                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    TransitionImageLayout(imageAllocation.image, vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::ImageSubresourceLayers imgSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    vk::BufferImageCopy region{
        0,
        0,
        0,
        imgSubresourceLayers,
        {0, 0, 0},
        vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1}};

    auto commandBuffer = VulkanRenderer::BeginSingleTimeCommands();
    commandBuffer.copyBufferToImage(stagingBufferAllocation.buffer, imageAllocation.image,
                                    vk::ImageLayout::eTransferDstOptimal, {region});
    VulkanRenderer::EndSingleTimeCommands(commandBuffer);

    TransitionImageLayout(imageAllocation.image, vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    s_Allocator.m_StagingBuffers.push(stagingBufferAllocation);
    return imageAllocation;
}

AccelerationAllocation Allocator::CreateAcceleration(vk::AccelerationStructureCreateInfoNV asInfo)
{
    AccelerationAllocation alloc;
    alloc.accel = s_Allocator.m_Device.createAccelerationStructureNV(asInfo);

    vk::AccelerationStructureMemoryRequirementsInfoNV memInfo;
    memInfo.accelerationStructure = alloc.accel;
    vk::MemoryRequirements2 memReqs =
        s_Allocator.m_Device.getAccelerationStructureMemoryRequirementsNV(memInfo);

    vk::MemoryAllocateInfo memAlloc;
    memAlloc.setAllocationSize(memReqs.memoryRequirements.size);
    memAlloc.setMemoryTypeIndex(FindMemoryType(memReqs.memoryRequirements.memoryTypeBits,
                                               vk::MemoryPropertyFlagBits::eDeviceLocal,
                                               s_Allocator.m_PhysicalDevice));
    alloc.allocation = s_Allocator.m_Device.allocateMemory(memAlloc);

    vk::BindAccelerationStructureMemoryInfoNV bind;
    bind.setAccelerationStructure(alloc.accel);
    bind.setMemory(alloc.allocation);
    bind.setMemoryOffset(0);
    s_Allocator.m_Device.bindAccelerationStructureMemoryNV(bind);

    return alloc;
}
