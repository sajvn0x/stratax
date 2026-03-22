#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 ourColor;

void main() {
    gl_Position = ubo.proj * ubo.view * vec4(aPos, 1.0);
    ourColor = aColor;
}
