#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 payLoad;

vec4 rgb2srgb(vec4 linearRGB)
{
    bvec4 cutoff = lessThan(linearRGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(linearRGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = linearRGB * vec4(12.92);

    return mix(higher, lower, cutoff);
}

void main()
{
    payLoad = rgb2srgb(vec4(0.1f, 0.1f, 0.1f, 1.0f)).rgb;
}
