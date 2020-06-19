#version 450 core

layout (std140) uniform FragmentUniforms {
    uint window_width;
    float divider;
};

bool is_left_of_divider() {
    float ndc_x = (gl_FragCoord.x / window_width) * 2.0f - 1.0f;
    return ndc_x < divider;
}
