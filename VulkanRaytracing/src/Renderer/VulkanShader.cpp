#include "vkpch.h"

#include "VulkanShader.h"

#include "Tools/FileTools.h"

VulkanShader::VulkanShader(vk::Device device) : m_Device(device)
{
}

void VulkanShader::LoadFromFile(const std::string &fileName, vk::ShaderStageFlagBits shaderStage)
{
    std::vector<char> code = ReadFile(fileName);
    vk::ShaderModuleCreateInfo createInfo{
        {}, code.size(), reinterpret_cast<const uint32_t *>(code.data())};
    m_Module = m_Device.createShaderModuleUnique(createInfo);
    m_Stage = shaderStage;
}

vk::PipelineShaderStageCreateInfo VulkanShader::GetShaderStageCreateInfo() const
{
    return {{}, m_Stage, m_Module.get(), "main"};
}
