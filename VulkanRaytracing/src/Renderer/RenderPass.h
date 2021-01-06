#include <vulkan/vulkan.hpp>

namespace util
{
    vk::RenderPass CreateRenderPass(
        const vk::Device device, const vk::Format colorAttachmentFormat,
        vk::SampleCountFlagBits samples, bool clearColor, vk::ImageLayout colorInitialLayout,
        vk::ImageLayout colorFinalLayout, vk::Format depthAttachmentFormat = vk::Format::eUndefined,
        bool clearDepth = true, vk::ImageLayout depthInitialLayout = vk::ImageLayout::eUndefined,
        vk::ImageLayout depthFinalLayout = vk::ImageLayout::eUndefined, bool resolve = false);
} // namespace util