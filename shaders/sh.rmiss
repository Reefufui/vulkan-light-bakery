#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "sh_common.h"

struct SHPayload
{
    vec3  sum;
    uint  lmax;
    ivec3 ijk;
    bool  occluded;
};

layout(location = 0) rayPayloadInEXT SHPayload pl;
layout(set = 0, binding = 4, scalar) buffer SHCoeffs { vec3 sh[]; } shCoeffs;

void main()
{
    ivec3 probesCount = ivec3(5);

    uint shCount  = (pl.lmax + 1u) * (pl.lmax + 1u);
    uint shOffset = shCount * (pl.ijk.x + pl.ijk.y * probesCount.x + pl.ijk.z * probesCount.x * probesCount.y);

    for (int l = 0; l < pl.lmax; l++)
    {
        for (int m = -l; m < l + 1; m++)
        {
            pl.sum += 250.0f * shCoeffs.sh[shOffset +  l * (l + 1) + m] * SH(l, m, gl_WorldRayDirectionEXT);
        }
    }

    pl.occluded = false;
}
