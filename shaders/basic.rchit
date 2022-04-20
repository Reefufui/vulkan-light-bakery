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
    vec3 a;
};

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 color;
layout(location = 1) rayPayloadEXT   bool inShadow;
layout(location = 2) rayPayloadEXT   SHPayload dummy; // TODO

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, scalar) buffer Instances { InstanceInfo i[]; } instanceInfo;
layout(set = 0, binding = 2, scalar) buffer Materials { Material     m[]; } materials;
layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Vertices  { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices   { ivec3  i[]; };

vec3 lightPos = vec3(5.0f, 5.0f, 5.0f);

vec4 sRGB(vec4 RGB)
{
    bvec4 cutoff = lessThan(RGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(RGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = RGB * vec4(12.92);

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

    const vec3 shadowRay = lightPos - hitPosition;

    const float ambient = 0.01f;
    float diffuse = 0.0f;
    float specular = 0.0f;

    const float sDotN = max(dot(normalize(shadowRay), hitNormal), 0.0f);

    inShadow = true;
    if (sDotN != 0.0f)
    {
        uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, hitPosition, 0.001f, normalize(shadowRay), length(shadowRay), 1);
    }

    if (!inShadow)
    {
        const float Cdiffuse  = 0.4f;
        diffuse   = Cdiffuse * sDotN;

        const float Cspecular = 0.4f;
        const float glossyness = 15.0f;
        const vec3  reflected = reflect(normalize(shadowRay), hitNormal);
        specular  = Cspecular * pow(max(dot(reflected, gl_WorldRayDirectionEXT), 0.0f), glossyness);
    }

    vec3 gridStep = vec3(0.04f, 0.02f, 0.03f); // TODO
    vec3 ijk = floor(hitPosition / gridStep);
    //
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

    for (int i = 0; i < 9; ++i)
    {
        vec3 dir = gridStep * (ijk + gridVertices[i]) - hitPosition;
        uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        //traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 2, hitPosition, 0.001f, normalize(dir), length(dir), 2);
    }

    color = sRGB(baseColor * (ambient + diffuse + specular)).rgb;
}

