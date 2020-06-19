#version 450 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;

out vec3 frag_pos;
out vec3 frag_norm;

uniform mat4 model;
uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(pos, 1.0);
    frag_pos = vec3(model * vec4(pos, 1.0));

    // TODO: Move calculation to CPU
    frag_norm = mat3(transpose(inverse(model))) * norm;
}
