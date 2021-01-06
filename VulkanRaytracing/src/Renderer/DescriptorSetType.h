#pragma once

#include <vulkan/vulkan.hpp>

class DescriptorSetType
{
  public:
    void Init(vk::Device device);
    void CreateDescriptorSetLayout(const std::vector<vk::DescriptorSetLayoutBinding> &bindings);
    void CreateDescriptorSets(vk::DescriptorPool pool, uint32_t count);

    vk::WriteDescriptorSet CreateWrite(const size_t binding, const vk::DescriptorBufferInfo *info,
                                       const uint32_t arrayElement);

    vk::WriteDescriptorSet CreateWrite(const size_t binding,
                                       const vk::DescriptorImageInfo *info,
                                       const uint32_t arrayElement);

    vk::WriteDescriptorSet CreateWrite(const size_t binding,
                                       const vk::WriteDescriptorSetAccelerationStructureNV *info,
                                       const uint32_t arrayElement);

    void Update(std::vector<vk::WriteDescriptorSet> &descriptorWrites);

    const std::vector<vk::DescriptorSet> &Get() const
    {
        return m_Sets;
    }

    const vk::DescriptorSetLayout &GetLayout() const
    {
        return m_Layout.get();
    }

  private:
    vk::Device m_Device;
    std::vector<vk::DescriptorSetLayoutBinding> m_Bindings;
    vk::UniqueDescriptorSetLayout m_Layout;
    std::vector<vk::DescriptorSet> m_Sets;
};
