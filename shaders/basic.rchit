#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable

#define BASIC_RCHIT

#include "structures.h"

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 payLoad;

layout(set = 0, binding = 1, scalar) buffer Instances { InstanceInfo i[]; } instanceInfo;
layout(set = 0, binding = 2, scalar) buffer Materials { Material     m[]; } materials;
layout(set = 0, binding = 3) uniform sampler2DArray textures;

layout(buffer_reference, scalar) buffer Vertices  { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices   { ivec3  i[]; };

vec3 lightPos = vec3(20.0f, 20.0f, 20.0f);

vec4 rgb2srgb(vec4 linearRGB)
{
    bvec4 cutoff = lessThan(linearRGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(linearRGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = linearRGB * vec4(12.92);

    return mix(higher, lower, cutoff);
}

void main()
{
    InstanceInfo info     = instanceInfo.i[gl_InstanceCustomIndexEXT];
    Indices      indices  = Indices(info.indexBufferAddress);
    Vertices     vertices = Vertices(info.vertexBufferAddress);
    Material     material = materials.m[int(info.materialIndex)];

    ivec3 ind = indices.i[gl_PrimitiveID];

    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    const vec4 bcCoords = vec4(1.0f - attribs.x - attribs.y, attribs.x, attribs.y, 1.0f);

    // Computing the coordinates of the hit position
    const vec3 pos      = v0.position.xyz * bcCoords.x + v1.position.xyz * bcCoords.y + v2.position.xyz * bcCoords.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    // Computing the normal at hit position
    const vec3 nrm      = v0.normal * bcCoords.x + v1.normal * bcCoords.y + v2.normal * bcCoords.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));

    const vec3 toLight = lightPos - worldPos;

    const vec4 diffuse = vec4(1.0f) * max(dot(worldNrm, normalize(toLight)), 0.0f);

    const vec4 colors[7] = vec4[](
            vec4(0.0f, 0.0f, 1.0f, 1.0f),
            vec4(0.0f, 1.0f, 0.0f, 1.0f),
            vec4(0.0f, 1.0f, 1.0f, 1.0f),
            vec4(1.0f, 0.0f, 0.0f, 1.0f),
            vec4(1.0f, 0.0f, 1.0f, 1.0f),
            vec4(1.0f, 1.0f, 0.0f, 1.0f),
            vec4(1.0f, 1.0f, 1.0f, 1.0f)
            );

    vec2 uv = v0.uv0 * bcCoords.x + v1.uv0 * bcCoords.y + v2.uv0 * bcCoords.z;

    int colorIdx = int(material.textures.baseColor.index);
    //int colorIdx = int(info.materialIndex);
    //int colorIdx = int(gl_InstanceCustomIndexEXT);

    vec4 color = vec4(0.05f) + constants.lightIntensity * diffuse;
    //color = color * texture(textures, vec3(uv, material.textures.baseColor.index));
    color = color * colors[colorIdx % 7];

    payLoad = rgb2srgb(color).rgb;

}

