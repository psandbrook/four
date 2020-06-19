#include <four/app_state.hpp>

#include <four/math.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <loguru.hpp>

#include <SDL.h>

namespace four {

namespace {

const f64 mouse_motion_fac = 0.002;
const ImWchar glyph_ranges[] = {0x20, 0xFFFF, 0};

inline bool imgui_drag_f64(const char* label, f64* value, f32 speed, const char* format = NULL) {
    return ImGui::DragScalar(label, ImGuiDataType_Double, value, speed, NULL, NULL, format, 1.0f);
}

f32 srgb_to_linear(f32 value) {
    if (value < 0.04045f) {
        return value / 12.92f;
    } else {
        return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
}
} // namespace

f64 AppState::screen_x(f64 x) {
    return x * window_width;
}

f64 AppState::screen_y(f64 y) {
    return y * window_height;
}

void AppState::change_mesh(const char* path) {
    mesh = load_mesh_from_file(path);
    mesh_pos = {0.0, 0.0, 0.0, 0.0};
    mesh_scale = {1, 1, 1, 1};
    if (mesh_rotation.is_rotor) {
        mesh_rotation.rotor = rotor4();
    } else {
        mesh_rotation.euler = (Bivec4){};
    }
    selected_cell = 0;
    mesh_changed = true;

    new_mesh_pos = mesh_pos;
    new_mesh_scale = mesh_scale;
    new_mesh_rotation = mesh_rotation;
}

AppState::AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path)
        : window(window), imgui_io(imgui_io), random_dev(), random_eng_32(random_dev()), mesh_rotation(rotation4()) {

    SDL_GL_GetDrawableSize(window, &window_width, &window_height);
    change_mesh(mesh_path);
    CHECK_NOTNULL_F(imgui_io->Fonts->AddFontFromFileTTF("data/DejaVuSans.ttf", 18.0f, NULL, glyph_ranges));

    ImGui::GetStyle().WindowRounding = 0;
    ImGui::GetStyle().ChildRounding = 6;
    ImGui::GetStyle().FrameRounding = 4;
    ImGui::GetStyle().GrabRounding = 2;
    ImGui::GetStyle().WindowBorderSize = 0;
    ImGui::GetStyle().WindowPadding = ImVec2(6, 6);

    ImVec4* colors = ImGui::GetStyle().Colors;
    for (s32 i = 0; i < ImGuiCol_COUNT; i++) {
        colors[i].x = srgb_to_linear(colors[i].x);
        colors[i].y = srgb_to_linear(colors[i].y);
        colors[i].z = srgb_to_linear(colors[i].z);
    }
}

bool AppState::process_events_and_imgui() {

    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        ImGui_ImplSDL2_ProcessEvent(&event);
        if (imgui_io->WantCaptureKeyboard && event.type == SDL_KEYDOWN) {
            continue;
        }
        if (imgui_io->WantCaptureMouse && (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL)) {
            continue;
        }

        switch (event.type) {
        case SDL_QUIT:
            return true;

        case SDL_WINDOWEVENT: {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GL_GetDrawableSize(window, &window_width, &window_height);
                window_size_changed = true;
            }
        } break;

        case SDL_KEYDOWN: {
            if (!event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return true;
                }
            }
        } break;

        case SDL_MOUSEMOTION: {
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {

                if (SDL_GetModState() & KMOD_SHIFT) {
                    // Pan the 3D camera

                    hmm_vec3 front = camera_target - camera_pos;
                    f64 distance_fac = 0.25 * HMM_Length(front);

                    hmm_vec3 f = HMM_Normalize(front);
                    hmm_vec3 left = HMM_Normalize(HMM_Cross(HMM_Vec3(0, 1, 0), f));
                    hmm_vec3 up = HMM_Cross(f, left);

                    f64 x_move = mouse_motion_fac * distance_fac * event.motion.xrel;
                    f64 y_move = mouse_motion_fac * distance_fac * event.motion.yrel;

                    hmm_mat4 y_trans = HMM_Translate(y_move * up);
                    hmm_mat4 x_trans = HMM_Translate(x_move * left);
                    hmm_mat4 translation = y_trans * x_trans;

                    camera_pos = transform(translation, camera_pos);
                    camera_target = transform(translation, camera_target);

                } else {
                    // Rotate the 3D camera

                    f64 x_angle = mouse_motion_fac * event.motion.xrel;
                    Rotor3 x_rotor = rotor3(x_angle, outer(HMM_Vec3(0, 0, -1), HMM_Vec3(1, 0, 0)));

                    f64 y_angle = mouse_motion_fac * event.motion.yrel;
                    Rotor3 y_rotor = rotor3(y_angle, outer(HMM_Vec3(0, 1, 0), camera_target - camera_pos));

                    hmm_mat4 rotation = to_mat4(y_rotor * x_rotor);
                    hmm_mat4 m = HMM_Translate(camera_target) * rotation * HMM_Translate(-1 * camera_target);

                    hmm_vec3 new_camera_pos = transform(m, camera_pos);
                    hmm_vec3 front = HMM_Normalize(camera_target - new_camera_pos);
                    if (!float_eq(std::abs(front.Y), 1.0, 0.001)) {
                        camera_pos = new_camera_pos;
                    }
                }
            }
        } break;

        case SDL_MOUSEWHEEL: {
            // Zoom the 3D camera

            f64 y = event.wheel.y;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                y *= -1;
            }

            hmm_vec3 front = camera_target - camera_pos;
            f64 distance_fac = 0.1 * HMM_Length(front);
            hmm_vec3 f = HMM_Normalize(front);
            hmm_mat4 translation = HMM_Translate(y * distance_fac * f);
            camera_pos = transform(translation, camera_pos);

        } break;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    if (debug) {
        ImGui::ShowDemoWindow();
    }

    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
                                          | ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2((f32)screen_x(visualization_width), 0));
    ImGui::SetNextWindowSize(ImVec2((f32)screen_x(1.0 - visualization_width), (f32)window_height));
    ImGui::SetNextWindowBgAlpha(0xff);
    ImGui::Begin("four", NULL, window_flags);

    hmm_vec4 prev_new_mesh_pos = new_mesh_pos;
    hmm_vec4 prev_new_mesh_scale = new_mesh_scale;
    Rotation4 prev_new_mesh_rotation = new_mesh_rotation;

    ImGui::BeginChild("ui_left", ImVec2(ImGui::GetContentRegionAvailWidth() * 0.5f, 0), true, window_flags);

    ImGui::Checkbox("Split", &split);

    ImGui::Spacing();
    ImGui::Separator();

    {
        const f32 speed = 0.01f;
        const auto fmt = "%.3f";

        ImGui::Text("Translate");
        imgui_drag_f64("x##t", &new_mesh_pos.X, speed, fmt);
        imgui_drag_f64("y##t", &new_mesh_pos.Y, speed, fmt);
        imgui_drag_f64("z##t", &new_mesh_pos.Z, speed, fmt);
        imgui_drag_f64("w##t", &new_mesh_pos.W, speed, fmt);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Scale");

        auto button_size = ImVec2(ImGui::GetContentRegionAvailWidth(), 0);
        ImGui::Button("xyzw", button_size);
        if (ImGui::IsItemActive()) {
            f64 scale_magnitude =
                    (std::abs(mesh_scale.X) + std::abs(mesh_scale.Y) + std::abs(mesh_scale.Z) + std::abs(mesh_scale.W))
                    / 4.0;
            f64 scale_speed = speed * scale_magnitude;

            f64 xyzw_scale = scale_speed * imgui_io->MouseDelta.x;
            new_mesh_scale.X += xyzw_scale;
            new_mesh_scale.Y += xyzw_scale;
            new_mesh_scale.Z += xyzw_scale;
            new_mesh_scale.W += xyzw_scale;
        }

        imgui_drag_f64("x##s", &new_mesh_scale.X, speed, fmt);
        imgui_drag_f64("y##s", &new_mesh_scale.Y, speed, fmt);
        imgui_drag_f64("z##s", &new_mesh_scale.Z, speed, fmt);
        imgui_drag_f64("w##s", &new_mesh_scale.W, speed, fmt);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Rotate");

        if (ImGui::BeginTabBar("RotationType")) {
            if (ImGui::BeginTabItem("Euler")) {
                if (new_mesh_rotation.is_rotor) {
                    new_mesh_rotation.is_rotor = false;
                    new_mesh_rotation.euler = rotor_to_euler(new_mesh_rotation.rotor);
                }
                imgui_drag_f64("xy", &new_mesh_rotation.euler.xy, speed, fmt);
                imgui_drag_f64("xz", &new_mesh_rotation.euler.xz, speed, fmt);
                imgui_drag_f64("xw", &new_mesh_rotation.euler.xw, speed, fmt);
                imgui_drag_f64("yz", &new_mesh_rotation.euler.yz, speed, fmt);
                imgui_drag_f64("yw", &new_mesh_rotation.euler.yw, speed, fmt);
                imgui_drag_f64("zw", &new_mesh_rotation.euler.zw, speed, fmt);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Rotor")) {
                if (!new_mesh_rotation.is_rotor) {
                    new_mesh_rotation.is_rotor = true;
                    new_mesh_rotation.rotor = euler_to_rotor(new_mesh_rotation.euler);
                }
                imgui_drag_f64("s", &new_mesh_rotation.rotor.s, speed, fmt);
                imgui_drag_f64("xy", &new_mesh_rotation.rotor.B.xy, speed, fmt);
                imgui_drag_f64("xz", &new_mesh_rotation.rotor.B.xz, speed, fmt);
                imgui_drag_f64("xw", &new_mesh_rotation.rotor.B.xw, speed, fmt);
                imgui_drag_f64("yz", &new_mesh_rotation.rotor.B.yz, speed, fmt);
                imgui_drag_f64("yw", &new_mesh_rotation.rotor.B.yw, speed, fmt);
                imgui_drag_f64("zw", &new_mesh_rotation.rotor.B.zw, speed, fmt);
                imgui_drag_f64("xyzw", &new_mesh_rotation.rotor.xyzw, speed, fmt);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("ui_right", ImVec2(0, 0), true, window_flags);

    {
        const char* new_mesh_path = NULL;
        ImGui::Text("Load");

        auto button_size = ImVec2(ImGui::GetContentRegionAvailWidth(), 0);
        if (ImGui::Button("5-cell", button_size)) {
            new_mesh_path = "5cell.mesh4";
        }
        if (ImGui::Button("Tesseract", button_size)) {
            new_mesh_path = "tesseract.mesh4";
        }
        if (ImGui::Button("16-cell", button_size)) {
            new_mesh_path = "16cell.mesh4";
        }
        if (ImGui::Button("24-cell", button_size)) {
            new_mesh_path = "24cell.mesh4";
        }
        if (ImGui::Button("120-cell", button_size)) {
            new_mesh_path = "120cell.mesh4";
        }
        if (ImGui::Button("600-cell", button_size)) {
            new_mesh_path = "600cell.mesh4";
        }

        if (new_mesh_path) {
            change_mesh(new_mesh_path);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    {
        ImGui::Text("Selected cell");
        ImGui::Checkbox("Cycle", &selected_cell_cycle);

        ImGui::PushItemWidth(-1);
        auto list_box_size = ImVec2(0, ImGui::GetContentRegionAvail().y);
        if (ImGui::ListBoxHeader("##empty", list_box_size)) {
            for (s32 i = 0; i < (s32)mesh.cells.size(); i++) {

                const auto fmt_str = "%i";
                s32 str_size = snprintf(NULL, 0, fmt_str, i);
                selected_cell_str.resize((size_t)str_size + 1);
                snprintf(selected_cell_str.data(), selected_cell_str.size(), fmt_str, i);

                if (ImGui::Selectable(selected_cell_str.data(), selected_cell == i)) {
                    selected_cell = i;
                }
            }
            ImGui::ListBoxFooter();
        }
        ImGui::PopItemWidth();
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::EndFrame();

    Mat5 model = mk_model_mat(new_mesh_pos, new_mesh_scale, new_mesh_rotation);
    Mat5 mv = mk_model_view_mat(model, camera4);
    for (const auto& v : mesh.vertices) {
        Vec5 view_v = mv * vec5(v, 1);
        if (view_v.W > -camera4.near) {
            new_mesh_pos = prev_new_mesh_pos;
            new_mesh_scale = prev_new_mesh_scale;
            new_mesh_rotation = prev_new_mesh_rotation;
            break;
        }
    }

    if (!imgui_io->WantTextInput) {
        mesh_pos = new_mesh_pos;
        mesh_scale = new_mesh_scale;
        mesh_rotation = new_mesh_rotation;
    }

    if (float_eq(std::round(mesh_pos.W), mesh_pos.W)) {
        bump_mesh_pos_w();
    }

    return false;
}

void AppState::step(const f64 ms) {

    if (selected_cell_cycle) {
        selected_cell_cycle_acc += ms;
        if (selected_cell_cycle_acc >= 2000.0) {
            selected_cell++;
            if (selected_cell == (s32)mesh.cells.size()) {
                selected_cell = 0;
            }
            selected_cell_cycle_acc = 0.0;
        }
    } else {
        selected_cell_cycle_acc = 0.0;
    }
}

void AppState::bump_mesh_pos_w() {
    const f64 mag = 0.0000001;

    // We bump towards zero because we assume that the hyperplane used for
    // cross-section visualization is at w = 0.
    if (mesh_pos.W <= 0.001) {
        mesh_pos.W += mag;
    } else {
        mesh_pos.W -= mag;
    }

    if (new_mesh_pos.W <= 0.001) {
        new_mesh_pos.W += mag;
    } else {
        new_mesh_pos.W -= mag;
    }

    LOG_F(WARNING, "New mesh w: %.16f", mesh_pos.W);
}

Mat5 mk_model_mat(const hmm_vec4& pos, const hmm_vec4& v_scale, const Rotation4& rotation) {
    Mat5 m_r;
    if (rotation.is_rotor) {
        m_r = to_mat5(rotation.rotor);
    } else {
        m_r = rotate_euler(rotation.euler);
    }

    return translate(pos) * m_r * scale(v_scale);
}

Mat5 mk_model_view_mat(const Mat5& model, const Camera4& camera) {
    Mat5 view = look_at(camera.pos, camera.target, camera.up, camera.over);
    return view * model;
}
} // namespace four
