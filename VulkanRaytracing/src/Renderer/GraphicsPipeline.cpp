#include "vkpch.h"

#include "GraphicsPipeline.h"

void GraphicsPipeline::Init(vk::Device device)
{
    m_Device = device;
}

void GraphicsPipeline::LoadVertexShader(std::string file)
{
    m_Shaders.emplace_back(m_Device);
    m_Shaders[m_Shaders.size() - 1].LoadFromFile(file, vk::ShaderStageFlagBits::eVertex);
}

void GraphicsPipeline::LoadFragmentShader(std::string file)
{
    m_Shaders.emplace_back(m_Device);
    m_Shaders[m_Shaders.size() - 1].LoadFromFile(file, vk::ShaderStageFlagBits::eFragment);
}

void GraphicsPipeline::CreatePipelineLayout(std::vector<vk::DescriptorSetLayout> descLayouts,
                                            std::vector<vk::PushConstantRange> pushConstRanges)
{
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{{},
                                                    static_cast<uint32_t>(descLayouts.size()),
                                                    descLayouts.data(),
                                                    static_cast<uint32_t>(pushConstRanges.size()),
                                                    pushConstRanges.data()};
    m_Layout = m_Device.createPipelineLayoutUnique(pipelineLayoutInfo);
}

void GraphicsPipeline::CreatePipeline(
    vk::RenderPass renderPass, vk::SampleCountFlagBits samples, vk::Extent2D extent,
    std::vector<vk::VertexInputBindingDescription> bindingDesc,
    std::vector<vk::VertexInputAttributeDescription> attributeDesc, vk::CullModeFlagBits cullMode)
{
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        {},
        static_cast<uint32_t>(bindingDesc.size()),
        bindingDesc.data(),
        static_cast<uint32_t>(attributeDesc.size()),
        attributeDesc.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE};

    vk::Viewport viewport{
        0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
        0.0f, 1.0f};

    vk::Rect2D scissor{{0, 0}, extent};

    vk::PipelineViewportStateCreateInfo viewportState{{}, 1, &viewport, 1, &scissor};

    vk::PipelineRasterizationStateCreateInfo rasterizer{{},       VK_FALSE,
                                                        VK_FALSE, vk::PolygonMode::eFill,
                                                        cullMode, vk::FrontFace::eCounterClockwise,
                                                        VK_FALSE, 0.0f,
                                                        0.0f,     0.0f,
                                                        1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{{},      samples,  VK_FALSE, 1.0f,
                                                         nullptr, VK_FALSE, VK_FALSE};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{VK_FALSE,
                                                               vk::BlendFactor::eOne,
                                                               vk::BlendFactor::eZero,
                                                               vk::BlendOp::eAdd,
                                                               vk::BlendFactor::eOne,
                                                               vk::BlendFactor::eZero,
                                                               vk::BlendOp::eAdd,
                                                               vk::ColorComponentFlagBits::eR |
                                                                   vk::ColorComponentFlagBits::eG |
                                                                   vk::ColorComponentFlagBits::eB};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        {}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        {}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f};

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (const auto &shader : m_Shaders)
    {
        shaderStages.push_back(shader.GetShaderStageCreateInfo());
    }

    vk::GraphicsPipelineCreateInfo pipelineInfo{{},
                                                static_cast<uint32_t>(shaderStages.size()),
                                                shaderStages.data(),
                                                &vertexInputInfo,
                                                &inputAssembly,
                                                {},
                                                &viewportState,
                                                &rasterizer,
                                                &multisampling,
                                                &depthStencil,
                                                &colorBlending,
                                                {},
                                                m_Layout.get(),
                                                renderPass,
                                                0};

    m_Pipeline = m_Device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
    m_Shaders.clear();
}
