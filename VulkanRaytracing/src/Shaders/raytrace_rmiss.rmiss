#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "raycommon.glsl"

layout(location = 0) rayPayloadInNV hitPayload prd;

layout(binding = 2) uniform sampler2D skysphereTextureSampler;
layout(binding = 3) uniform sampler2D hdrSkysphereTextureSampler;

layout(push_constant) uniform Constants
{
    vec4 clearColor;
    vec3 lightPosition;
    float lightIntensity;
    int lightType;
    int nSamples;
    int hdr;
}
pushC;

const float pi = 3.14159265;

void main()
{
    vec3 dir = gl_WorldRayDirectionNV;
    vec2 texCoord;
    texCoord.x = 0.5 + atan(dir.z, dir.x) / (2 * pi);
    texCoord.y = 0.5 - asin(dir.y) / pi;
    vec3 envColor;
    if (pushC.hdr == 1)
    {
        envColor = texture(hdrSkysphereTextureSampler, texCoord).xyz;
        envColor = pow(envColor, vec3(0.6));
    }
    else
    {
        envColor = texture(skysphereTextureSampler, texCoord).xyz;
    }
    prd.hitValue = envColor * prd.attenuation;
    /*int texIndex;
    if(abs(dir.x) > abs(dir.y)) 
    {
        if(abs(dir.x) > abs(dir.z)) 
        {
            if(dir.x < 0) 
            {
                texIndex = 4;
                texCoord.x = dir.z / (-dir.x);
                texCoord.y = -dir.y / (-dir.x);
            } 
            else 
            {
                texIndex = 3;
                texCoord.x = -dir.z / (dir.x);
                texCoord.y = -dir.y / (dir.x);
            }
        } 
        else 
        {
            if (dir.z < 0)
            {
                texIndex = 0;
                texCoord.x = -dir.x / (-dir.z);
                texCoord.y = -dir.y / (-dir.z);
            }
            else
            {
                texIndex = 2;
                texCoord.x = dir.x / (dir.z);
                texCoord.y = -dir.y / (dir.z);
            }
        }
    } 
    else if(abs(dir.y) > abs(dir.z)) 
    {
        if (dir.y < 0)
        {
            texIndex = 1;
            texCoord.x = dir.x / (-dir.y);
            texCoord.y = -dir.z / (-dir.y);
        }
        else
        {
            texIndex = 5;
            texCoord.x = dir.x / (dir.y);
            texCoord.y = dir.z / (dir.y);
        }
    } 
    else
    {
        if (dir.z < 0)
        {
            texIndex = 0;
            texCoord.x = -dir.x / (-dir.z);
            texCoord.y = -dir.y / (-dir.z);
        }
        else
        {
            texIndex = 2;
            texCoord.x = dir.x / (dir.z);
            texCoord.y = -dir.y / (dir.z);
        }
    }
    prd.done = 1;
    texCoord.x = (texCoord.x + 1) / 2;
    texCoord.y = (texCoord.y + 1) / 2;
    prd.hitValue = texture(skyboxTextureSamplers[texIndex], texCoord).xyz * prd.attenuation;*/
}