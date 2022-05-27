#version 460
#extension GL_EXT_ray_tracing : enable

#define PI 3.1415926538f

layout(location = 0) rayPayloadInEXT vec3 payLoad;
layout(set = 3, binding = 0) uniform sampler2D skybox;

vec4 sRGB(vec4 linearRGB)
{
    bvec4 cutoff = lessThan(linearRGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(linearRGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = linearRGB * vec4(12.92);

    return mix(higher, lower, cutoff);
}

vec2 dir2SkyboxUV(const vec3 dir)
{
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float phi = atan(dir.x, dir.z);

    theta = mod(theta, 2.0f * PI);
    theta = clamp(theta, 0.0f, 2.0f * PI);
    if (theta > PI)
    {
        theta = 2.0f * PI - theta;
        phi += PI;
    }

    phi = mod(phi, 2.0f * PI);
    phi = clamp(phi, 0.0f, 2.0f * PI);

    return vec2(phi / (2.0f * PI), theta / PI);
}

void main()
{
    vec4 color = texture(skybox, dir2SkyboxUV(gl_WorldRayDirectionEXT.xyz));
    payLoad = sRGB(color).rgb;
}
