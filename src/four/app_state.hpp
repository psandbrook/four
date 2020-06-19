#pragma once

#include <four/math.hpp>
#include <four/mesh.hpp>

struct SDL_Window;
struct ImGuiIO;

namespace four {

struct Camera4 {
    hmm_vec4 pos = {0, 0, 0, 0};
    hmm_vec4 target = {0, 0, 0, 1};
    hmm_vec4 up = {0, 1, 0, 0};
    hmm_vec4 over = {0, 0, 1, 0};
    f64 near = 1.0;
};

struct Rotation4 {
    bool is_rotor;
    union {
        Bivec4 euler;
        Rotor4 rotor;
    };
};

inline Rotation4 rotation4() {
    Rotation4 result = {};
    result.is_rotor = false;
    result.euler = (Bivec4){};
    return result;
}

struct AppState {
public:
    SDL_Window* window;
    ImGuiIO* imgui_io;

    bool debug = false;
    bool window_size_changed = false;
    bool mesh_changed = false;

    Mesh4 mesh;
    hmm_vec4 mesh_pos;
    hmm_vec4 mesh_scale;
    Rotation4 mesh_rotation;

    s32 selected_cell;
    bool selected_cell_cycle = false;
    f64 selected_cell_cycle_acc = 0.0;

    Camera4 camera4;

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

private:
    void change_mesh(const char* path);
};

Mat5 mk_model_view_mat(const hmm_vec4& pos, const hmm_vec4& scale, const Rotation4& rotation, const Camera4& camera);

} // namespace four
