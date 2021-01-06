#pragma once

#include <queue>

#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>

struct AccelerationAllocation
{
    vk::AccelerationStructureNV accel;
    vk::DeviceMemory allocation;
};

struct BufferAllocation
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct ImageAllocation
{
    VkImage image;
    VmaAllocation allocation;
};

class Allocator
{
  public:
    static void Init(vk::PhysicalDevice physicalDevice, vk::Device device);
    static void FlushStaging();
    static BufferAllocation CreateBuffer(const vk::DeviceSize &size,
                                         const vk::BufferUsageFlags &usage,
                                         const VmaMemoryUsage &memoryUsage);

    static ImageAllocation CreateImage(const uint32_t width, const uint32_t height,
                                       vk::SampleCountFlagBits sampleCount,
                                       const vk::Format &format, const vk::ImageTiling &tiling,
                                       const vk::ImageUsageFlags &usage,
                                       const VmaMemoryUsage &memoryUsage);

    static void TransitionImageLayout(vk::Image image, vk::ImageAspectFlagBits aspect,
                                      vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

    static ImageAllocation CreateTextureImage(const std::string &filename);

    static ImageAllocation CreateHdrTextureImage(const std::string &filename);

    template <typename T>
    static void UpdateAllocation(const VmaAllocation &allocation, const T &data)
    {
        void *mappedData;
        vmaMapMemory(s_Allocator.m_Allocator, allocation, &mappedData);
        memcpy(mappedData, &data, sizeof(T));
        vmaUnmapMemory(s_Allocator.m_Allocator, allocation);
    }

    template <typename T>
    static BufferAllocation CreateDeviceLocalBuffer(const vk::CommandBuffer &commandBuffer,
                                                    const std::vector<T> &data,
                                                    const vk::BufferUsageFlags &usage)
    {
        vk::DeviceSize bufferSize = sizeof(data[0]) * data.size();

        BufferAllocation stagingBufferAllocation = CreateBuffer(
            bufferSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_GPU_TO_CPU);

        void *mappedData;
        vmaMapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation, &mappedData);
        memcpy(mappedData, data.data(), (size_t)bufferSize);
        vmaUnmapMemory(s_Allocator.m_Allocator, stagingBufferAllocation.allocation);

        BufferAllocation resultBufferAllocation = CreateBuffer(
            bufferSize, vk::BufferUsageFlagBits::eTransferDst | usage, VMA_MEMORY_USAGE_GPU_ONLY);

        vk::BufferCopy copyRegion{0, 0, bufferSize};

        commandBuffer.copyBuffer(stagingBufferAllocation.buffer, resultBufferAllocation.buffer, 1,
                                 &copyRegion);
        s_Allocator.m_StagingBuffers.push(stagingBufferAllocation);
        return resultBufferAllocation;
    }

    static AccelerationAllocation CreateAcceleration(vk::AccelerationStructureCreateInfoNV asInfo);

  private:
    Allocator() = default;
    Allocator(const Allocator &) = delete;
    Allocator &operator=(const Allocator &) = delete;

  private:
    static Allocator s_Allocator;

    VmaAllocator m_Allocator;
    vk::PhysicalDevice m_PhysicalDevice;
    vk::Device m_Device;
    std::queue<BufferAllocation> m_StagingBuffers;
};
