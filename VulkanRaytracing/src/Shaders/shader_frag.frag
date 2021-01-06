#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "material.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 fragCameraPos;
layout(location = 4) in vec2 fragTexCoord;
layout(location = 5) flat in int fragMatID;
layout(location = 6) in vec3 worldPos;
layout(location = 7) in vec3 viewDir;

layout(location = 0) out vec4 outColor;

layout(binding = 1, scalar) readonly buffer MaterialBufferObject { Material m[]; } materials[];
layout(binding = 2) uniform sampler2D textureSamplers[];
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
    int objId = instances.i[pushC.instanceID].objId;
    Material mat = materials[objId].m[fragMatID];

    vec3 N = normalize(fragNorm);

    // Vector toward light
    vec3  L;
    float lightIntensity = 50.0f;
    vec3  lDir     = pushC.lightPosition - worldPos;
    float d        = length(lDir);
    lightIntensity = 50.0f / (d * d);
    L              = normalize(lDir);

    // Diffuse
    vec3 diffuse = computeDiffuse(mat, L, N);
    if(mat.textureId >= 0)
    {
        int  txtOffset  = instances.i[pushC.instanceID].texOffset;
        uint txtId      = txtOffset + mat.textureId;
        vec3 diffuseTxt = texture(textureSamplers[txtId], fragTexCoord).xyz;
        diffuse *= diffuseTxt;
    }

    // Specular
    vec3 specular = computeSpecular(mat, viewDir, L, N);

    // Result
    vec3 outC = vec3(lightIntensity * (diffuse + specular));

    //   vec3 norm = normalize(fragNorm);
    //   vec3 lightDir = normalize(pushC.lightPosition - fragPos);

    //   float diff = max(dot(norm, lightDir), 0.0);
    //   vec3 diffuse = diff * pushC.lightColor;

    //   vec3 diffuseTxt = texture(textureSamplers[materials[pushC.instanceID].m[fragMatID].textureId + pushC.texOffset], fragTexCoord).xyz;
    //   diffuse *= diffuseTxt;
    //
    //   float specularStrength = 0.5;
    //   vec3 viewDir = normalize(fragCameraPos - fragPos);
    //   vec3 reflectDir = reflect(-lightDir, norm);
    //   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    //   vec3 specular = specularStrength * spec * pushC.lightColor;  

    //  vec3 result = (diffuse + specular);
    // outColor = vec4(1.0f, 0.0f, 0.0f, 0.0f);
        //outColor = vec4(materials[pushC.instanceID].m[0].shininess, materials[pushC.instanceID].m[0].ior, materials[pushC.instanceID].m[0].dissolve, 1.0f);
        //outColor = vec4(result, 1.0f);
    outColor = texture(textureSamplers[mat.textureId + instances.i[pushC.instanceID].texOffset], fragTexCoord).rgba;
}