#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define BASIC_RCHIT

#include "structures.h"

// TODO: move to common file
struct SHPayload
{
    vec3  sum;
    vec3  normal;
    ivec3 ijk;
    uint  lmax;
    bool  occluded;
};

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 color;
layout(location = 1) rayPayloadEXT   bool inShadow;
layout(location = 2) rayPayloadEXT   SHPayload shPayload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, scalar) buffer Instances { InstanceInfo i[]; } instanceInfo;
layout(set = 0, binding = 2, scalar) buffer Materials { Material     m[]; } materials;
layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Vertices  { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices   { ivec3  i[]; };

layout(push_constant, scalar) uniform PushConstants
{
    vec3  gridStep;
    uint  lmax;
    vec3  lightPosition;
    float shadowBias;
    float ambient;
    float Cdiffuse;
    float Cspecular;
    float Cglossyness;
} constants;

vec3 sRGB(vec3 RGB)
{
    bvec3 cutoff = lessThan(RGB, vec3(0.0031308));
    vec3 higher = vec3(1.055)*pow(RGB, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = RGB * vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec4 getBaseColor(Material material, vec2 uv)
{
    vec4 baseColor = vec4(1.0f); // TODO: ui
    int textureIdx = int(material.textures.baseColor.index);
    if (textureIdx != -1)
    {
        baseColor = texture(textures[textureIdx], uv);
    }
    else if (material.factors.baseColor != vec4(0.0f))
    {
        baseColor = material.factors.baseColor;
    }
    return baseColor;
}

void main()
{
    InstanceInfo instance = instanceInfo.i[gl_InstanceCustomIndexEXT];

    Indices indices = Indices(instance.indexBufferAddress);
    ivec3 index = indices.i[gl_PrimitiveID];

    Vertices vertices = Vertices(instance.vertexBufferAddress);
    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];

    const vec4 bc          = vec4(1.0f - attribs.x - attribs.y, attribs.x, attribs.y, 1.0f);
    const vec3 nrm         = v0.normal * bc.x + v1.normal * bc.y + v2.normal * bc.z;
    const vec2 uv          = v0.uv0 * bc.x + v1.uv0 * bc.y + v2.uv0 * bc.z;

    const vec3 hitPosition = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    const vec3 hitNormal   = normalize(vec3(nrm * gl_WorldToObjectEXT));

    Material material = materials.m[int(instance.materialIndex)];
    vec4 baseColor = getBaseColor(material, uv);

    const vec3 shadowRay = constants.lightPosition - hitPosition;

    float diffuse = 0.0f;
    float specular = 0.0f;

    const float sDotN = max(dot(normalize(shadowRay), hitNormal), 0.0f);

    const vec3  origin = hitPosition + constants.shadowBias * hitNormal;
    inShadow = true;
    if (sDotN != 0.0f)
    {
        uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, origin, 0.0f, normalize(shadowRay), length(shadowRay), 1);
    }

    if (!inShadow)
    {
        diffuse   = constants.Cdiffuse * sDotN;

        const vec3  reflected  = reflect(normalize(shadowRay), hitNormal);
        specular               = constants.Cspecular * pow(max(dot(reflected, gl_WorldRayDirectionEXT), 0.0f), constants.Cglossyness);
    }

    if (constants.gridStep != vec3(0.0f))
    {
        vec3 ijk = floor(hitPosition / constants.gridStep);

        const vec3 gridVertices[8] = vec3[](
                vec3(0.0f, 0.0f, 0.0f),
                vec3(0.0f, 0.0f, 1.0f),
                vec3(0.0f, 1.0f, 0.0f),
                vec3(0.0f, 1.0f, 1.0f),
                vec3(1.0f, 0.0f, 0.0f),
                vec3(1.0f, 0.0f, 1.0f),
                vec3(1.0f, 1.0f, 0.0f),
                vec3(1.0f, 1.0f, 1.0f)
                );

        vec3  shSum = vec3(0.0f);
        float weightSum = 0.0f;
        const float weightMax = length(constants.gridStep);

        for (int i = 0; i < 8; ++i)
        {
            const vec3 dir = constants.gridStep * (ijk + gridVertices[i]) - hitPosition;
            const uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            shPayload.ijk       = ivec3(ijk);
            shPayload.lmax      = constants.lmax;
            shPayload.sum       = vec3(0.0f);
            shPayload.normal    = hitNormal;
            shPayload.occluded  = true;

            const float tmax = length(dir);
            traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 2, origin, 0.0f, normalize(dir), tmax, 2);
            const float weight = weightMax - tmax;

            if (!shPayload.occluded)
            {
                shSum     += weight * shPayload.sum;
                weightSum += weight;
            }
        }

        shSum = shSum / weightSum;

        color = sRGB(baseColor.rgb * (constants.ambient * 1250.0f * shSum + vec3(diffuse + specular))).rgb;
    }
    else
    {
        color = sRGB(baseColor.rgb * (constants.ambient + vec3(diffuse + specular))).rgb;
    }
}

