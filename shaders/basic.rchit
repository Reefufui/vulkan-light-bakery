#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define BASIC_RCHIT

#include "structures.h"

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 payLoad;
layout(location = 1) rayPayloadEXT   bool inShadow;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, scalar) buffer Instances { InstanceInfo i[]; } instanceInfo;
layout(set = 0, binding = 2, scalar) buffer Materials { Material     m[]; } materials;
layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Vertices  { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices   { ivec3  i[]; };

vec3 lightPos = vec3(0.0f, 10.0f, 0.0f);

vec4 rgb2srgb(vec4 linearRGB)
{
    bvec4 cutoff = lessThan(linearRGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(linearRGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = linearRGB * vec4(12.92);

    return mix(higher, lower, cutoff);
}

const vec4 colors[7] = vec4[](
            vec4(0.0f, 0.0f, 1.0f, 1.0f),
            vec4(0.0f, 1.0f, 0.0f, 1.0f),
            vec4(0.0f, 1.0f, 1.0f, 1.0f),
            vec4(1.0f, 0.0f, 0.0f, 1.0f),
            vec4(1.0f, 0.0f, 1.0f, 1.0f),
            vec4(1.0f, 1.0f, 0.0f, 1.0f),
            vec4(1.0f, 1.0f, 1.0f, 1.0f)
            );

void main()
{
    InstanceInfo info     = instanceInfo.i[gl_InstanceCustomIndexEXT];
    Material     material = materials.m[int(info.materialIndex)];
    Indices      indices  = Indices(info.indexBufferAddress);
    Vertices     vertices = Vertices(info.vertexBufferAddress);

    ivec3 index = indices.i[gl_PrimitiveID];
    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];

    const vec4 bcCoords = vec4(1.0f - attribs.x - attribs.y, attribs.x, attribs.y, 1.0f);
    const vec3 pos      = v0.position.xyz * bcCoords.x + v1.position.xyz * bcCoords.y + v2.position.xyz * bcCoords.z;
    const vec3 worldPos = (gl_ObjectToWorldEXT * vec4(pos, 1.0)).xyz;
    const vec3 nrm      = v0.normal * bcCoords.x + v1.normal * bcCoords.y + v2.normal * bcCoords.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));
    const vec2 uv = v0.uv0 * bcCoords.x + v1.uv0 * bcCoords.y + v2.uv0 * bcCoords.z;

    const vec3 shadowRayDir = normalize(lightPos - worldPos);
    const vec4 diffuse = vec4(1.0f) * max(dot(worldNrm, shadowRayDir), 0.0f);

    inShadow = true;

    vec4 color = vec4(0.05f) + constants.lightIntensity * diffuse;

    int textureIdx = int(material.textures.baseColor.index);
    if (textureIdx != -1)
    {
        color = color * texture(textures[textureIdx], uv);
    }
    else if (material.factors.baseColor != vec4(0.0f))
    {
        color = material.factors.baseColor * color;
    }

    payLoad = rgb2srgb(color).rgb;
}

