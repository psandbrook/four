#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;

out vec3 frag_color;

layout (std140) uniform ViewProjection {
    mat4 vp;
};

void main() {
    frag_color = color;
    gl_Position = vp * vec4(pos, 1.0f);
}
