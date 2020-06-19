#version 450 core

layout (location = 0) in vec4 pos;

out float depth4;

layout (std140) uniform ViewProjection {
    mat4 vp;
};

void main() {
    depth4 = pos.w;
    gl_Position = vp * vec4(pos.xyz, 1.0f);
}
