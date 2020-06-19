#version 450 core

out vec4 out_color;

const float shade = 0.9f;

void main() {
    out_color = vec4(shade, shade, shade, 1.0f);
}
