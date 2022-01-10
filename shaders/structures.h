#ifndef STRUCTURES_H
#define STRUCTURES_H

#ifdef __cplusplus
namespace shader {

#include <glm/glm.hpp>
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;
#endif

#ifdef __cplusplus
    struct
#else
    layout(push_constant) uniform
#endif
        PushConstant
        {
            float lightIntensity;
        }
#ifndef __cplusplus
    constants
#endif
        ;

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

    struct Factors
    {
        vec4  baseColor;
        vec4  emissive;
        vec4  diffuse;
        vec3  specular;
        float metallic;
        float roughness;
        float dummy[3]; // alignment
    };

#ifdef __cplusplus
#define _MINUS_ONE = -1
#else
#define _MINUS_ONE
#endif

    struct Texture
    {
        int index _MINUS_ONE;
        int coordSet;
    };

    struct Textures
    {
        Texture normal;
        Texture occlusion;

        Texture baseColor;
        Texture metallicRoughness;
        Texture emissive;

        Texture diffuseEXT;
        Texture specularEXT;
        Texture dummy;
    };


    struct Material
    {
        Textures textures;
        Factors  factors;
    };

#ifdef __cplusplus
}
#endif

#endif // STRUCTURES_H

