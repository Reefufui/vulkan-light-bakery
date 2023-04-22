#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv0;
layout(location = 3) in vec2 inUv1;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 projection;
    mat4 view;
    mat4 projectionInv;
    mat4 viewInv;
} camera;

layout (location = 0) out VOUT
{
    vec3 normal;
    vec3 worldModel;
} vOut;

void main() {
    vec4 position = camera.projection * camera.view * inPosition;
    vOut.worldModel = position.xyz;
    gl_Position = position;

    vOut.normal = normalize(inNormal);
}
