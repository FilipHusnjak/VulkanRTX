#pragma once

#include <vulkan/vulkan.hpp>

namespace util
{
    vk::DescriptorPool CreateDescriptorPool(
        const vk::Device device, const std::vector<vk::DescriptorSetLayoutBinding> &bindings,
        const uint32_t maxSets);

    vk::DescriptorPool CreateDescriptorPool(const vk::Device device,
                                            const std::vector<vk::DescriptorPoolSize> &sizes,
                                            const uint32_t maxSets);
} // namespace util
