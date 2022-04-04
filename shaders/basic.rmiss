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
    vec3 color = vec3(2.0f) + vec3(0.0f, 0.0f, 5.0f * sin(gl_WorldRayDirectionEXT.y));
    payLoad = rgb2srgb(vec4(color, 1.0f)).rgb;
}
