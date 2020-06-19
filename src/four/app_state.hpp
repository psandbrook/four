#pragma once

#include <four/mesh.hpp>

#include <SDL.h>
#include <imgui.h>

namespace four {

struct AppState {
    SDL_Window* window;
    ImGuiIO* imgui_io;

    bool window_size_changed = false;

    Mesh4 mesh;
    bool mesh_changed = false;

    hmm_vec4 mesh_pos = {0, 0, 0, 2.5};
    s32 selected_cell = 0;

    hmm_vec4 camera4_pos = {0, 0, 0, 0};
    hmm_vec4 camera4_target = {0, 0, 0, 1};
    hmm_vec4 camera4_up = {0, 1, 0, 0};
    hmm_vec4 camera4_over = {0, 0, 1, 0};

    hmm_vec3 camera_pos = {0, 0, 4};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

    // Temporary storage
    // -----------------

    std::vector<char> selected_cell_str;

    // -----------------

    AppState() {}
    AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path);

    // Returns true if the application should exit
    bool process_events_and_imgui();

    void step(f64 ms);
};
} // namespace four
