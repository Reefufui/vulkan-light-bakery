#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

hitAttributeEXT vec3 attribs;
layout(location = 0) rayPayloadInEXT vec3 payLoad;

struct ObjDesc
{
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
};

struct Vertex
{
    vec4 position;
    vec3 normal;
    vec2 uv0;
    vec2 uv1;
};

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices {ivec3 i[]; };
layout(set = 0, binding = 1, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout(push_constant) uniform constants
{
    float lightIntensity;
    vec3 lightPos;
} pc;
vec3 lightPos = vec3(20.0f, 20.0f, 20.0f);

vec4 rgb2srgb(vec4 linearRGB)
{
    bvec4 cutoff = lessThan(linearRGB, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(linearRGB, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = linearRGB * vec4(12.92);

    return mix(higher, lower, cutoff);
}

void main()
{
    ObjDesc    objResource = objDesc.i[gl_InstanceCustomIndexEXT];
    Indices    indices     = Indices(objResource.indexBufferAddress);
    Vertices   vertices    = Vertices(objResource.vertexBufferAddress);

    ivec3 ind = indices.i[gl_PrimitiveID];

    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    const vec4 bcCoords = vec4(1.0f - attribs.x - attribs.y, attribs.x, attribs.y, 1.0f);

    // Computing the coordinates of the hit position
    const vec3 pos      = v0.position.xyz * bcCoords.x + v1.position.xyz * bcCoords.y + v2.position.xyz * bcCoords.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

    // Computing the normal at hit position
    const vec3 nrm      = v0.normal * bcCoords.x + v1.normal * bcCoords.y + v2.normal * bcCoords.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space

    const vec3 toLight = lightPos - worldPos;

    const vec4 diffuse = vec4(1.0f) * max(dot(worldNrm, normalize(toLight)), 0.0f);

    const vec4 color = vec4(0.05f) + pc.lightIntensity * diffuse;

    payLoad = rgb2srgb(color).rgb;
}
