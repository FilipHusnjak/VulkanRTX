#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "material.glsl"
#include "raycommon.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 1, scalar) readonly buffer MatColorBufferObject { Material m[]; } materials[];
layout(binding = 2, set = 1) uniform sampler2D textureSamplers[];
layout(binding = 3, set = 1, scalar) readonly buffer InstanceBufferObject { Instance i[]; } instances;
layout(binding = 4, set = 1, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(binding = 5, set = 1) buffer Indices { uint i[]; } indices[];

layout(location = 0) rayPayloadInNV hitPayload prd;
layout(location = 1) rayPayloadNV bool isShadowed;

layout(push_constant) uniform Constants
{
    vec4 clearColor;
    vec3 lightPosition;
    float lightIntensity;
    int lightType;
    int nSamples;
    bool hdr;
    float ni;
    float F0;
}
pushC;

hitAttributeNV vec3 attribs;

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}  

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main()
{
    // Object of this instance 
    uint objId = instances.i[gl_InstanceID].objId;

    // Indices of the triangle
    ivec3 ind = ivec3(indices[objId].i[3 * gl_PrimitiveID + 0],   //
                        indices[objId].i[3 * gl_PrimitiveID + 1],   //
                        indices[objId].i[3 * gl_PrimitiveID + 2]);  //
    // Vertex of the triangle
    Vertex v0 = vertices[objId].v[ind.x];
    Vertex v1 = vertices[objId].v[ind.y];
    Vertex v2 = vertices[objId].v[ind.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Computing the normal at hit position
    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    // Transforming the normal to world space
    normal = normalize(vec3(instances.i[gl_InstanceID].modelMatrixIT * vec4(normal, 0.0)));
    // Computing the coordinates of the hit position
    vec3 worldPos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
    // Transforming the position to world space
    worldPos = vec3(instances.i[gl_InstanceID].modelMatrix * vec4(worldPos, 1.0));

    // Vector toward the light
    vec3  L;
    float lightIntensity = pushC.lightIntensity;
    float lightDistance  = 100000.0;
    // Point light
    if(pushC.lightType == 0)
    {
        vec3 lDir = pushC.lightPosition - worldPos;
        lightDistance = length(lDir);
        lightIntensity = pushC.lightIntensity / (lightDistance * lightDistance);
        L = normalize(lDir);
    }
    else // Directional light
    {
        L = normalize(pushC.lightPosition - vec3(0));
    }

    // Material of the object
    Material mat = materials[objId].m[v0.matID];
    // Diffuse
    vec3 diffuse = computeDiffuse(mat, L, normal);
    if(mat.textureId >= 0)
    {
        uint txtId = mat.textureId + instances.i[gl_InstanceID].texOffset;
        vec2 texCoord =
            v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;
        diffuse *= texture(textureSamplers[txtId], texCoord).xyz;
    }
    
    // Specular
    vec3 specular = vec3(0);
    float attenuation = 1;
    specular = computeSpecular(mat, gl_WorldRayDirectionNV, L, normal);

    // Tracing shadow ray only if the light is visible from the surface
    /*if(dot(normal, L) > 0)
    {
        float tMin = 0.001;
        float tMax = lightDistance;
        vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
        vec3 rayDir = L;
        uint flags =
            gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV;
        isShadowed = true;
        traceNV(topLevelAS,  // acceleration structure
                flags,       // rayFlags
                0xFF,        // cullMask
                0,           // sbtRecordOffset
                0,           // sbtRecordStride
                1,           // missIndex
                origin,      // ray origin
                tMin,        // ray min range
                rayDir,      // ray direction
                tMax,        // ray max range
                1            // payload (location = 1)
        );
        isShadowed = false;

        if(isShadowed)
        {
            attenuation = 0.3;
        }
        else
        {
            specular = computeSpecular(mat, gl_WorldRayDirectionNV, L, normal);
        }
    }*/

    if(mat.illum == 3 && prd.depth < 5)
    {
        uint  rayFlags = gl_RayFlagsOpaqueNV;
        float tMin     = 0.001;
        float tMax     = 10000.0;

        prd.attenuation *= mat.specular;

        prd.depth++;

        if (false)
        {
            float roughness = 0.2;
            vec3 V = -normalize(gl_WorldRayDirectionNV);
            int SAMPLE_COUNT = 25;
            float totalWeight = 0.f;
            vec3 origin = worldPos + gl_WorldRayDirectionNV * 0.0001;
            vec3 prefilteredColor = vec3(0.0);     
            for(uint i = 0; i < SAMPLE_COUNT; ++i)
            {
                vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                vec3 H  = ImportanceSampleGGX(Xi, normal, roughness);
                vec3 L  = normalize(2.0 * dot(V, H) * H - V);

                float NdotL = max(dot(normal, L), 0.0);
                if(NdotL > 0.0)
                {
                    traceNV(topLevelAS,        // acceleration structure
                            rayFlags,          // rayFlags
                            0xFF,              // cullMask
                            0,                 // sbtRecordOffset
                            0,                 // sbtRecordStride
                            0,                 // missIndex
                            worldPos,            // ray origin
                            tMin,              // ray min range
                            L,            // ray direction
                            tMax,              // ray max range
                            0                  // payload (location = 0)
                    );
                    prefilteredColor += prd.hitValue * NdotL;
                    totalWeight      += NdotL;
                }
            }
            prefilteredColor = prefilteredColor / totalWeight;
            prd.hitValue = prefilteredColor;
        }
        else
        {
            vec3 V = -normalize(gl_WorldRayDirectionNV);
            vec3 H = normalize(V + L);
            float F = fresnelSchlick(max(dot(normal, V), 0.0), pushC.F0);
            const float NdotD = dot(normal, gl_WorldRayDirectionNV);

            vec3 refrNormal = normal;
            float refrIndex;

            if(NdotD > 0.0f) 
            {
                refrNormal = -normal;
                refrIndex = pushC.ni;
            } 
            else 
            {
                refrNormal = normal;
                refrIndex = 1 / pushC.ni;
            }
            vec3 origin = worldPos;
            vec3 rayDir = refract(gl_WorldRayDirectionNV, refrNormal, refrIndex);
            traceNV(topLevelAS,        // acceleration structure
                    rayFlags,          // rayFlags
                    0xFF,              // cullMask
                    0,                 // sbtRecordOffset
                    0,                 // sbtRecordStride
                    0,                 // missIndex
                    origin,            // ray origin
                    tMin,              // ray min range
                    rayDir,            // ray direction
                    tMax,              // ray max range
                    0                  // payload (location = 0)
            );
            vec3 refrColor = prd.hitValue;

            if (NdotD < 0.0f)
            {
                rayDir = reflect(gl_WorldRayDirectionNV, normal);
                traceNV(topLevelAS,      // acceleration structure
                    rayFlags,          // rayFlags
                    0xFF,              // cullMask
                    0,                 // sbtRecordOffset
                    0,                 // sbtRecordStride
                    0,                 // missIndex
                    origin,            // ray origin
                    tMin,              // ray min range
                    rayDir,            // ray direction
                    tMax,              // ray max range
                    0                  // payload (location = 0)
                );
                prd.hitValue = F * prd.hitValue + (1 - F) * refrColor;
            }
        }
        prd.depth--;
    }
    else
    {
        prd.hitValue = vec3(attenuation * lightIntensity * (diffuse + specular)) * prd.attenuation;
    }
}
