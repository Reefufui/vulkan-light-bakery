#ifndef STRUCTURES_H
#define STRUCTURES_H

#ifdef __cplusplus
#include <glm/glm.hpp>
using vec4 = glm::vec4;
using vec3 = glm::vec3;
using vec2 = glm::vec2;
#else
#endif

struct InstanceInfo
{
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint64_t materialIndex;
};

struct Vertex
{
    vec4 position;
    vec3 normal;
    vec2 uv0;
    vec2 uv1;
};

#endif // STRUCTURES_H

