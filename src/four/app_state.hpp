#pragma once

#include <four/math.hpp>
#include <four/mesh.hpp>

#include <random>

struct SDL_Window;
struct ImGuiIO;

namespace four {

inline constexpr s32 plane4_n = 6;

struct Camera4 {
    glm::dvec4 pos = {0, 0, 0, 4};
    glm::dvec4 target = {0, 0, 0, 0};
    glm::dvec4 up = {0, 1, 0, 0};
    glm::dvec4 over = {0, 0, 1, 0};
    f64 near = 1.0;
};

struct Rotation4 {
    bool is_rotor;
    union {
        Bivec4 euler;
        Rotor4 rotor;
    };
};

struct Transform4 {
    glm::dvec4 position;
    glm::dvec4 scale;
    Rotation4 rotation;
};

struct AppState {
public:
    SDL_Window* window;
    ImGuiIO* imgui_io;
    std::random_device random_dev;
    std::mt19937 random_eng_32;

    bool debug = false;
    bool wireframe_render = false;

    bool window_size_changed = false;

    struct MeshInstancesEvent {
        enum class Type { added, removed };
        Type type;
        u32 id;
    };

    std::vector<MeshInstancesEvent> mesh_instances_events;

    s32 window_width = 0;
    s32 window_height = 0;

    f64 visualization_width = 0.83;
    f64 ui_size_screen = 0;
    bool split = true;
    f64 divider = 0.5;

    std::vector<Mesh4> meshes;

    struct MeshInstance {
        u32 mesh_index;
        Transform4 transform;
        std::array<bool, plane4_n> auto_rotate;
        std::array<f64, plane4_n> auto_rotate_magnitude;
    };

    u32 next_mesh_instance_id = 0;
    std::vector<u32> mesh_instances_insertion;
    std::unordered_map<u32, MeshInstance> mesh_instances;

    u32 selected_mesh_instance = 0;

    bool selected_cell_enabled = false;
    u32 selected_cell = 0;
    bool selected_cell_cycle = false;
    f64 selected_cell_cycle_acc = 0;

    bool perspective_projection = true;
    Camera4 camera4 = {};

    glm::dvec3 camera_pos = {-1.5, 2, 3.5};
    glm::dvec3 camera_target = {0, 0, 0};
    glm::dvec3 camera_up = {0, 1, 0};

    glm::dvec4 cross_section_p_0 = {0, 0, 0, 0};
    glm::dvec4 cross_section_n = {0, 0, 0, 1};

private:
    bool dragging_ui = false;
    bool dragging_divider = false;

    u32 n5cell_index;
    u32 tesseract_index;
    u32 n16cell_index;
    u32 n24cell_index;
    u32 n120cell_index;
    u32 n600cell_index;

    MeshInstance dummy_mesh_instance = {};

    // Temporary storage
    // -----------------

    std::vector<char> str_buffer;

    // -----------------

public:
    AppState(SDL_Window* window, ImGuiIO* imgui_io);

    // Returns true if the application should exit
    bool process_events_and_imgui();

    void step(f64 ms);

    f64 screen_x(f64 x);
    f64 screen_y(f64 y);
    f64 norm_x(f64 x);
    void bump_mesh_pos_w(u32 mesh_instance);
    MeshInstance& get_selected_mesh_instance();
    Mesh4& get_mesh(u32 mesh_instance);
    Transform4& get_transform(u32 mesh_instance);

private:
    u32 mesh_with_name(const char* name);
    void add_mesh_instance(u32 mesh_index);
    void remove_mesh_instance(u32 mesh_instance);
    void set_selected_mesh_instance(u32 mesh_instance);
    bool is_mouse_around_x(f64 x);
    void calc_ui_size_screen();
    void validate_mesh_transform(MeshInstance& mesh_instance, const Transform4& old_transform);
};

Mat5 mk_model_mat(const Transform4& transform4);
Mat5 mk_model_view_mat(const Mat5& model, const Camera4& camera);

} // namespace four
