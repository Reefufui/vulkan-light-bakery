#version 450

layout(set = 1, binding = 0) uniform sampler2D skybox;

layout (location = 0) in VOUT
{
    vec3 normal;
    vec3 worldModel;
} vIn;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldLight = vec3(1.0f);
    vec3 toLight = worldLight - vIn.worldModel;

    vec4 diffuse = vec4(1.0f) * max(dot(vIn.normal, normalize(toLight)), 0.0f);

    outColor = vec4(0.5f, 0.1f, 0.6f, 1.0f) + diffuse * vec4(1.0f);
}
