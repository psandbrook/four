#version 450 core

in vec3 frag_color;

out vec4 out_color;

bool is_left_of_divider();

void main() {
    if (!is_left_of_divider()) {
        discard;
    }

    out_color = vec4(frag_color, 1.0f);
}
