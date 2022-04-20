#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct SHPayload
{
    ivec3  ijk;
    vec3  sum;
    float nrmCoef;
};

layout(location = 0) rayPayloadInEXT SHPayload pl;
layout(set = 0, binding = 4, scalar) buffer SHCoeffs { float sh[]; } shCoeffs;

int lmax = 16;

void main()
{
    ivec3 probesCount = ivec3(5);

    //float color = shCoeffs[lmax * (pl.ijk.x + pl.ijk.y * probesCount.x + pl.ijk.z * probesCount.x * probesCount.y)];
}
