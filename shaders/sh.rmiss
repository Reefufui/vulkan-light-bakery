#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 payLoad;

void main()
{
    vec3 color = vec3(0.3f) + vec3(mix(0.0f, 1.0f, gl_WorldRayDirectionEXT.y), 0.0f, 0.0f);
    payLoad = rgb2srgb(vec4(color, 1.0f)).rgb;
}
