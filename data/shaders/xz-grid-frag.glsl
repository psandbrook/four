#version 450 core

out vec4 out_color;

uniform bool is_projection;

const float shade = 0.23f;

bool is_left_of_divider();

void main() {
    if ((is_left_of_divider() && is_projection) || (!is_left_of_divider() && !is_projection)) {
        discard;
    }

    out_color = vec4(shade, shade, shade, 1.0f);
}
