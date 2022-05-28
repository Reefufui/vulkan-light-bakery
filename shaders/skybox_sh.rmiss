#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "sh_common.h"

layout(location = 0) rayPayloadInEXT vec3 skyboxRadiance;
layout(set = 3, binding = 1, scalar) buffer SHCoeffs { vec3 sh[]; } shCoeffs;

void main()
{
    vec3 hitNormal = normalize(vec3(gl_WorldRayDirectionEXT));

    for (int l = 0; l < 16; l++)
    {
        for (int m = -l; m < l + 1; m++)
        {
            skyboxRadiance += shCoeffs.sh[l * (l + 1) + m] * SH(l, m, hitNormal);
        }
    }
}
