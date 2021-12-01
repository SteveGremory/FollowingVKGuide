#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 outColor;

// binding = 0 says that pick the binding bound at 0
// set = 0 says that pick up the first one in the binding
layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewproj;
    mat4 rotation;
} cameraData;

// read object data from storage buffer
struct ObjectData {
    mat4 model;
}

// We need the std140 layout description to make the array match how arrays work in cpp.
// That std140 enforces some rules about how the memory is laid out, and what is its alignment.
// Also, using readonly buffer and not uniform as:
// 1. To tell Vulkan only to read from it
// 2. as Storage buffers are defined as buffers even though they're uniforms.
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer(
    objectData[] objects;
) objectBuffer;

void main() {
    mat4 transformMatrix = objectBuffer.objects[gl_BaseInstance].model;
    gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    outColor = vColor;
}
