#version 450 core

out vec4 out_color;

void main() {
    const float shade = 0.23f;
    out_color = vec4(shade, shade, shade, 1.0f);
}
