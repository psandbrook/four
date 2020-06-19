#version 450 core

in float depth4;

out vec4 out_color;

uniform float max_depth;
uniform vec3 color1;

const vec3 color0 = vec3(1.0f, 0.0f, 0.0f);
const vec3 color2 = vec3(1.0f, 1.0f, 1.0f);

bool is_left_of_divider();

void main() {
    if (is_left_of_divider()) {
        discard;
    }

    float fac10 = clamp(0.3f / depth4 - 0.3f, 0.0f, 1.0f);

    vec3 color;
    if (fac10 == 0.0f) {
        float fac12 = clamp((depth4 - 1.0f) / (max_depth - 1.0f), 0.0f, 1.0f);
        color = color1 + fac12 * (color2 - color1);
    } else {
        color = color1 + fac10 * (color0 - color1);
    }

    out_color = vec4(color, 1.0f);
}
