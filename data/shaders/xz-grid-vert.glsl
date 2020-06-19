#version 450 core

layout (location = 0) in vec3 pos;

layout (std140) uniform ViewProjection {
    mat4 vp;
};

void main() {
    gl_Position = vp * vec4(pos, 1.0f);
}
