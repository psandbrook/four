#version 330 core

layout (location = 0) in vec3 pos;

uniform float x_pos;

void main() {
    gl_Position = vec4(pos.x + x_pos, pos.y, pos.z, 1.0f);
}
