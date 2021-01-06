#include "vkpch.h"

#include "DescriptorPool.h"

vk::DescriptorPool util::CreateDescriptorPool(
    const vk::Device device, const std::vector<vk::DescriptorSetLayoutBinding> &bindings,
    const uint32_t maxSets)
{
    std::vector<vk::DescriptorPoolSize> sizes;
    sizes.reserve(bindings.size());
    for (const auto &b : bindings)
    {
        sizes.emplace_back(b.descriptorType, b.descriptorCount * maxSets);
    }
    return util::CreateDescriptorPool(device, sizes, maxSets);
}

vk::DescriptorPool util::CreateDescriptorPool(const vk::Device device,
                                              const std::vector<vk::DescriptorPoolSize> &sizes,
                                              const uint32_t maxSets)
{
    vk::DescriptorPoolCreateInfo poolInfo = {};
    poolInfo.setPoolSizeCount(static_cast<uint32_t>(sizes.size()));
    poolInfo.setPPoolSizes(sizes.data());
    poolInfo.setMaxSets(maxSets);

    vk::DescriptorPool pool = device.createDescriptorPool(poolInfo);
    assert(pool);
    return pool;
}
