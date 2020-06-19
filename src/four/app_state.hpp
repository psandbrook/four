#pragma once

#include <four/mesh.hpp>

namespace four {

struct AppState {
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

    const f64 camera_move_units_per_sec = 1.0;
    bool camera_move_up = false;
    bool camera_move_down = false;
    bool camera_move_forward = false;
    bool camera_move_backward = false;
    bool camera_move_left = false;
    bool camera_move_right = false;
};

AppState new_app_state(const char* mesh_path);

// Returns true if the application should exit
bool handle_events(AppState& state);

void step_state(AppState& state, f64 ms);

} // namespace four
