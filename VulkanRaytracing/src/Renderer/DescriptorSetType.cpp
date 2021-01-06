#include "vkpch.h"

#include "DescriptorSetType.h"

void DescriptorSetType::Init(vk::Device device)
{
    m_Device = device;
}

void DescriptorSetType::CreateDescriptorSetLayout(
    const std::vector<vk::DescriptorSetLayoutBinding> &bindings)
{
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, static_cast<uint32_t>(bindings.size()),
                                                 bindings.data());
    m_Layout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);
    m_Bindings = bindings;
    assert(m_Layout && m_Layout.get());
}

void DescriptorSetType::CreateDescriptorSets(vk::DescriptorPool pool, uint32_t count)
{
    std::vector<vk::DescriptorSetLayout> layouts(count, m_Layout.get());
    vk::DescriptorSetAllocateInfo allocInfo(pool, count, layouts.data());
    m_Sets = m_Device.allocateDescriptorSets(allocInfo);
    assert(m_Sets.data());
}

vk::WriteDescriptorSet DescriptorSetType::CreateWrite(const size_t binding,
                                                      const vk::DescriptorBufferInfo *info,
                                                      const uint32_t arrayElement)
{
    return {{},
            m_Bindings[binding].binding,
            arrayElement,
            m_Bindings[binding].descriptorCount,
            m_Bindings[binding].descriptorType,
            nullptr,
            info};
}

vk::WriteDescriptorSet DescriptorSetType::CreateWrite(const size_t binding,
                                                      const vk::DescriptorImageInfo *info,
                                                      const uint32_t arrayElement)
{
    return {{},
            m_Bindings[binding].binding,
            arrayElement,
            m_Bindings[binding].descriptorCount,
            m_Bindings[binding].descriptorType,
            info};
}

vk::WriteDescriptorSet DescriptorSetType::CreateWrite(
    const size_t binding, const vk::WriteDescriptorSetAccelerationStructureNV *info,
    const uint32_t arrayElement)
{
    vk::WriteDescriptorSet res({}, m_Bindings[binding].binding, arrayElement,
                               m_Bindings[binding].descriptorCount,
                               m_Bindings[binding].descriptorType);
    res.setPNext(info);
    return res;
}

void DescriptorSetType::Update(std::vector<vk::WriteDescriptorSet> &descriptorWrites)
{
    assert(descriptorWrites.size() == m_Bindings.size());
    for (const auto &ds : m_Sets)
    {
        for (auto &wr : descriptorWrites)
        {
            wr.setDstSet(ds);
        }
        m_Device.updateDescriptorSets(descriptorWrites, nullptr);
    }
}
