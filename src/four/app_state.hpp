#pragma once

#include <four/math.hpp>
#include <four/mesh.hpp>

#include <random>

struct SDL_Window;
struct ImGuiIO;

namespace four {

struct Camera4 {
    hmm_vec4 pos = {0, 0, 0, 4};
    hmm_vec4 target = {0, 0, 0, 0};
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
    std::random_device random_dev;
    std::mt19937 random_eng_32;

    bool debug = false;
    bool mesh_changed = false;
    bool window_size_changed = false;

    s32 window_width;
    s32 window_height;
    f64 visualization_width = 0.85;
    f64 divider = 0.5;

    Mesh4 mesh;
    hmm_vec4 mesh_pos;
    hmm_vec4 mesh_scale;
    Rotation4 mesh_rotation;

    s32 selected_cell;
    bool selected_cell_cycle = false;
    f64 selected_cell_cycle_acc = 0.0;

    Camera4 camera4;

    hmm_vec3 camera_pos = {-1.5, 2, 3.5};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

    hmm_vec4 new_mesh_pos;
    hmm_vec4 new_mesh_scale;
    Rotation4 new_mesh_rotation;

    // Temporary storage
    // -----------------

    std::vector<char> selected_cell_str;

    // -----------------

    AppState() {}
    AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path);

    // Returns true if the application should exit
    bool process_events_and_imgui();

    void step(f64 ms);

    f64 screen_x(f64 x);
    f64 screen_y(f64 y);
    void bump_mesh_pos_w();

private:
    void change_mesh(const char* path);
};

Mat5 mk_model_mat(const hmm_vec4& pos, const hmm_vec4& scale, const Rotation4& rotation);
Mat5 mk_model_view_mat(const Mat5& model, const Camera4& camera);

} // namespace four
