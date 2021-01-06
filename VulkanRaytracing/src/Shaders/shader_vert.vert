#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "material.glsl"

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 texCoord;
layout(location = 4) in int matID;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 fragCameraPos;
layout(location = 4) out vec2 fragTexCoord;
layout(location = 5) flat out int fragMatID;
layout(location = 6) out vec3 worldPos;
layout(location = 7) out vec3 viewDir;

layout(binding = 0, scalar) uniform CameraMatrices 
{
    vec3 cameraPos;
    mat4 view;
    mat4 projection;
    mat4 viewInverse;
    mat4 projectionInverse;
} cameraMatrices;

layout(binding = 3, scalar) readonly buffer InstanceBufferObject { Instance i[]; } instances;

layout(push_constant, scalar) uniform PushConstant
{
    int instanceID;
    vec3 lightPosition;
    vec3 lightColor;
}
pushC;

void main()
{
    mat4 modelMatrix = instances.i[pushC.instanceID].modelMatrix;
    gl_Position = cameraMatrices.projection * cameraMatrices.view * modelMatrix * vec4(pos, 1.0);
    fragColor = color;
    fragNorm = norm;
    fragPos = pos;
    fragCameraPos = cameraMatrices.cameraPos;
    fragTexCoord = texCoord;
    fragMatID = matID;
    worldPos = vec3(modelMatrix * vec4(pos, 1.0));
    vec3 origin = vec3(cameraMatrices.view * vec4(0, 0, 0, 1));
    viewDir      = vec3(worldPos - origin);
}