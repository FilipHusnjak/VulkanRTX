#pragma once

#include <vulkan/vulkan.hpp>

class VulkanShader
{
  public:
    VulkanShader(vk::Device device);

    void LoadFromFile(const std::string &fileName, vk::ShaderStageFlagBits shaderStage);

    vk::PipelineShaderStageCreateInfo GetShaderStageCreateInfo() const;

  private:
    vk::Device m_Device;
    vk::UniqueShaderModule m_Module;
    vk::ShaderStageFlagBits m_Stage;
};
