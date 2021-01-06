#include "vkpch.h"

#include "ObjModel.h"

#include "Core/Core.h"
#include "Tools/FileTools.h"

#include "VulkanRenderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

std::vector<TextureImage> ObjModel::s_TextureImages;
std::vector<TextureImage> ObjModel::s_SkyboxTextureImages;
TextureImage ObjModel::s_Skysphere;
TextureImage ObjModel::s_HdrSkysphere;

vk::VertexInputBindingDescription Vertex::getBindingDescription()
{
    return {0, sizeof(Vertex)};
}

std::vector<vk::VertexInputAttributeDescription> Vertex::getAttributeDescriptions()
{
    return {{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
            {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, norm)},
            {2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)},
            {3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)},
            {4, 0, vk::Format::eR32Sint, offsetof(Vertex, matID)}};
}

static inline std::string GetPath(const std::string &file)
{
    std::string dir;
    size_t idx = file.find_last_of("\\/");
    if (idx != std::string::npos)
        dir = file.substr(0, idx);
    if (!dir.empty())
    {
        dir += "\\";
    }
    return dir;
}

void ObjModel::LoadSkysphere()
{
    std::string skysphereTexture = "birchwood.jpg";
    ImageAllocation imgAllocation =
        Allocator::CreateTextureImage("textures/" + skysphereTexture);
    vk::ImageView textureImageView = VulkanRenderer::CreateImageView(
        imgAllocation.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    vk::SamplerCreateInfo samplerInfo = {
        {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.compareOp = vk::CompareOp::eNever;

    samplerInfo.setMaxLod(FLT_MAX);
    vk::Sampler sampler = VulkanRenderer::CreateSampler(samplerInfo);
    vk::DescriptorImageInfo desc{sampler, textureImageView,
                                 vk::ImageLayout::eShaderReadOnlyOptimal};
    s_Skysphere = {desc, imgAllocation};

    Allocator::FlushStaging();
}

void ObjModel::LoadHdrSkysphere()
{
    std::string skysphereTexture = "birchwood_16k.hdr";
    ImageAllocation imgAllocation =
        Allocator::CreateHdrTextureImage("textures/" + skysphereTexture);
    vk::ImageView textureImageView = VulkanRenderer::CreateImageView(
        imgAllocation.image, vk::Format::eR32G32B32Sfloat, vk::ImageAspectFlagBits::eColor);
    vk::SamplerCreateInfo samplerInfo = {
        {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.compareOp = vk::CompareOp::eNever;

    samplerInfo.setMaxLod(FLT_MAX);
    vk::Sampler sampler = VulkanRenderer::CreateSampler(samplerInfo);
    vk::DescriptorImageInfo desc{sampler, textureImageView,
                                 vk::ImageLayout::eShaderReadOnlyOptimal};
    s_HdrSkysphere = {desc, imgAllocation};

    Allocator::FlushStaging();
}

ObjModel ObjModel::LoadModel(const std::string &filePath)
{
    std::vector<Material> materials;
    std::vector<std::string> textures;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> tinyMaterials;
    std::string warn, err;

    result_assert(tinyobj::LoadObj(&attrib, &shapes, &tinyMaterials, &warn, &err, filePath.c_str(),
                                   GetPath(filePath).c_str()));

    for (const auto &material : tinyMaterials)
    {
        Material m = {};
        m.ambient = glm::vec3(material.ambient[0], material.ambient[1], material.ambient[2]);
        m.diffuse = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
        m.specular = glm::vec3(material.specular[0], material.specular[1], material.specular[2]);
        m.emission = glm::vec3(material.emission[0], material.emission[1], material.emission[2]);
        m.transmittance = glm::vec3(material.transmittance[0], material.transmittance[1],
                                    material.transmittance[2]);
        m.dissolve = material.dissolve;
        m.ior = material.ior;
        m.shininess = material.shininess;
        m.illum = material.illum;
        if (!material.diffuse_texname.empty())
        {
            textures.push_back(material.diffuse_texname);
            m.textureID = static_cast<int>(textures.size()) - 1;
        }

        materials.emplace_back(m);
    }

    if (materials.empty())
    {
        materials.emplace_back(Material());
    }

    // Converting from Srgb to linear
    for (auto &m : materials)
    {
        m.ambient = glm::pow(m.ambient, glm::vec3(2.2f));
        m.diffuse = glm::pow(m.diffuse, glm::vec3(2.2f));
        m.specular = glm::pow(m.specular, glm::vec3(2.2f));
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto &shape : shapes)
    {
        uint32_t faceID = 0;
        int indexCount = 0;
        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex = {};
            vertex.pos = {attrib.vertices[3L * static_cast<uint64_t>(index.vertex_index) + 0L],
                          attrib.vertices[3L * static_cast<uint64_t>(index.vertex_index) + 1L],
                          attrib.vertices[3L * static_cast<uint64_t>(index.vertex_index) + 2L]};

            if (!attrib.normals.empty() && index.normal_index >= 0)
            {
                vertex.norm = {attrib.normals[3L * static_cast<uint64_t>(index.normal_index) + 0L],
                               attrib.normals[3L * static_cast<uint64_t>(index.normal_index) + 1L],
                               attrib.normals[3L * static_cast<uint64_t>(index.normal_index) + 2L]};
            }

            if (!attrib.colors.empty())
            {
                vertex.color = {attrib.colors[3L * static_cast<uint64_t>(index.vertex_index) + 0L],
                                attrib.colors[3L * static_cast<uint64_t>(index.vertex_index) + 1L],
                                attrib.colors[3L * static_cast<uint64_t>(index.vertex_index) + 2L]};
            }

            if (!attrib.texcoords.empty() && index.texcoord_index >= 0)
            {
                vertex.texCoord = {
                    attrib.texcoords[2L * static_cast<uint64_t>(index.texcoord_index) + 0L],
                    1.0f - attrib.texcoords[2L * static_cast<uint64_t>(index.texcoord_index) + 1L]};
            }

            vertex.matID = shape.mesh.material_ids[faceID];
            if (vertex.matID < 0 || vertex.matID >= materials.size())
            {
                vertex.matID = 0;
            }
            indexCount++;
            if (indexCount >= 3)
            {
                ++faceID;
                indexCount = 0;
            }

            vertices.push_back(vertex);
            indices.push_back(static_cast<int>(indices.size()));
        }
    }

    if (attrib.normals.empty())
    {
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            Vertex &v0 = vertices[indices[i + 0]];
            Vertex &v1 = vertices[indices[i + 1]];
            Vertex &v2 = vertices[indices[i + 2]];

            glm::vec3 n = glm::normalize(glm::cross((v1.pos - v0.pos), (v2.pos - v0.pos)));
            v0.norm = n;
            v1.norm = n;
            v2.norm = n;
        }
    }

    ObjModel objModel;
    objModel.verticesCount = (uint32_t)vertices.size();
    objModel.indicesCount = (uint32_t)indices.size();
    auto cmdBuff = VulkanRenderer::BeginSingleTimeCommands();
    objModel.vertexBuffer = Allocator::CreateDeviceLocalBuffer(
        cmdBuff, vertices,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    objModel.indexBuffer = Allocator::CreateDeviceLocalBuffer(
        cmdBuff, indices,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    objModel.materialBuffer = Allocator::CreateDeviceLocalBuffer(
        cmdBuff, materials, vk::BufferUsageFlagBits::eStorageBuffer);
    objModel.textureOffset = static_cast<uint32_t>(s_TextureImages.size());
    VulkanRenderer::EndSingleTimeCommands(cmdBuff);
    for (auto &texturePath : textures)
    {
        ImageAllocation imgAllocation = Allocator::CreateTextureImage("textures/" + texturePath);
        vk::ImageView textureImageView = VulkanRenderer::CreateImageView(
            imgAllocation.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
        vk::SamplerCreateInfo samplerInfo = {
            {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        samplerInfo.setMaxLod(FLT_MAX);
        vk::Sampler sampler = VulkanRenderer::CreateSampler(samplerInfo);
        vk::DescriptorImageInfo desc{sampler, textureImageView,
                                     vk::ImageLayout::eShaderReadOnlyOptimal};
        s_TextureImages.push_back({desc, imgAllocation});
    }

    if (textures.empty())
    {
        ImageAllocation imgAllocation = Allocator::CreateTextureImage("");
        vk::ImageView textureImageView = VulkanRenderer::CreateImageView(
            imgAllocation.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
        vk::SamplerCreateInfo samplerInfo = {
            {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        samplerInfo.setMaxLod(FLT_MAX);
        vk::Sampler sampler = VulkanRenderer::CreateSampler(samplerInfo);
        vk::DescriptorImageInfo desc{sampler, textureImageView,
                                     vk::ImageLayout::eShaderReadOnlyOptimal};
        s_TextureImages.push_back({desc, imgAllocation});
    }

    Allocator::FlushStaging();
    return objModel;
}
