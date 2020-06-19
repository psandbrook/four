#pragma once

#include <four/mesh.hpp>

namespace four {

struct AppState {
    bool window_size_changed = false;

    Mesh4 mesh;

    hmm_vec4 mesh_pos = {0, 0, 0, 2.5};
    u32 selected_cell = 0;

    hmm_vec4 camera4_pos = {0, 0, 0, 0};
    hmm_vec4 camera4_target = {0, 0, 0, 1};
    hmm_vec4 camera4_up = {0, 1, 0, 0};
    hmm_vec4 camera4_over = {0, 0, 1, 0};

    hmm_vec3 camera_pos = {0, 0, 4};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};
};

AppState new_app_state(const char* mesh_path);

// Returns true if the application should exit
bool handle_events(AppState& state);

void step_state(AppState& state, f64 ms);

} // namespace four
