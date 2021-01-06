#pragma once

#include "Core/Allocator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec3 color;
    glm::vec2 texCoord;
    int matID;

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex &other) const
    {
        return pos == other.pos;
    }
};

struct Material
{
    glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    glm::vec3 diffuse = glm::vec3(0.7f, 0.7f, 0.7f);
    glm::vec3 specular = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 transmittance = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 emission = glm::vec3(0.0f, 0.0f, 0.1f);
    float shininess = 0.f;
    float ior = 1.0f;     // index of refraction
    float dissolve = 1.f; // 1 == opaque; 0 == fully transparent
    int illum = 1;
    int textureID = 0;
};

struct ObjInstance
{
    uint32_t objModelIndex{0};
    glm::mat4 modelMatrix{1};
    glm::mat4 modelMatrixIT{1};
    uint32_t textureOffset;
};

struct TextureImage
{
    vk::DescriptorImageInfo descriptor;
    ImageAllocation textureAllocation;
};

struct ObjModel
{
  public:
    uint32_t indicesCount{0};
    uint32_t verticesCount{0};
    BufferAllocation vertexBuffer;
    BufferAllocation indexBuffer;
    BufferAllocation materialBuffer;
    uint32_t textureOffset;

  public:
    ObjModel() = default;
    static void LoadSkysphere();
    static void LoadHdrSkysphere();
    static ObjModel LoadModel(const std::string &filename);
    static std::vector<TextureImage> s_TextureImages;
    static std::vector<TextureImage> s_SkyboxTextureImages;
    static TextureImage s_Skysphere;
    static TextureImage s_HdrSkysphere;
};