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
    Bivec4 rot = {};
    result.euler = rot;
    return result;
}

enum class Plane {
    xy,
    xz,
    xw,
    yz,
    yw,
    zw,
    count,
};

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

    f64 visualization_width = 0.83;
    f64 ui_size_screen;
    bool split = true;
    f64 divider = 0.5;

    Mesh4 mesh;
    hmm_vec4 mesh_pos;
    hmm_vec4 mesh_scale;
    Rotation4 mesh_rotation;

    s32 selected_cell;
    bool selected_cell_cycle;
    f64 selected_cell_cycle_acc;

    bool auto_rotate[(size_t)Plane::count];
    f64 auto_rotate_mag[(size_t)Plane::count];

    bool perspective_projection = true;
    Camera4 camera4;

    hmm_vec3 camera_pos = {-1.5, 2, 3.5};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

private:
    hmm_vec4 new_mesh_pos;
    hmm_vec4 new_mesh_scale;
    Rotation4 new_mesh_rotation;
    Camera4 new_camera4;

    bool dragging_ui = false;
    bool dragging_divider = false;

    // Temporary storage
    // -----------------

    std::vector<char> selected_cell_str;

    // -----------------

public:
    AppState() {}
    AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path);

    // Returns true if the application should exit
    bool process_events_and_imgui();

    void step(f64 ms);

    f64 screen_x(f64 x);
    f64 screen_y(f64 y);
    f64 norm_x(f64 x);
    void bump_mesh_pos_w();

private:
    void change_mesh(const char* path);
    bool is_mouse_around_x(f64 x);
    bool is_new_transformation_valid();
    void apply_new_transformation(const hmm_vec4& prev_pos, const hmm_vec4& prev_scale, const Rotation4& prev_rotation,
                                  const Camera4& prev_camera4);
    void calc_ui_size_screen();
};

Mat5 mk_model_mat(const hmm_vec4& pos, const hmm_vec4& scale, const Rotation4& rotation);
Mat5 mk_model_view_mat(const Mat5& model, const Camera4& camera);

} // namespace four
