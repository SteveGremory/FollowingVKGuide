#version 450

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

// Push constant block
layout (push_constant) uniform constants {
    vec4 data;
    mat4 render_matrix;
} PushConstants;

void main() {
    mat4 transformMatrix = (cameraData.viewproj * PushConstants.render_matrix);
    gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    outColor = vColor;
}
