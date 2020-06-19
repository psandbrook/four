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

inline bool imgui_drag_f64(const char* label, f64* value, f32 speed, const char* format = NULL) {
    return ImGui::DragScalar(label, ImGuiDataType_Double, value, speed, NULL, NULL, format, 1.0f);
}
} // namespace

void AppState::change_mesh(const char* path) {
    mesh = load_mesh_from_file(path);
    mesh_pos = {0, 0, 0, 2.5};
    mesh_scale = {1, 1, 1, 1};
    mesh_rotation = (Bivec4){.xy = 0, .xz = 0, .xw = 0, .yz = 0, .yw = 0, .zw = 0};
    selected_cell = 0;
    mesh_changed = true;
}

AppState::AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path) : window(window), imgui_io(imgui_io) {
    change_mesh(mesh_path);

    ImWchar ranges[] = {0x20, 0xFFFF, 0};
    CHECK_NOTNULL_F(imgui_io->Fonts->AddFontFromFileTTF("data/DejaVuSans.ttf", 18.0f, NULL, ranges));
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

#ifdef FOUR_DEBUG
    ImGui::ShowDemoWindow();
#endif

    // Mesh selection window
    {
        ImGui::Begin("Mesh selection");
        const char* new_mesh_path = NULL;

        if (ImGui::Button("5-cell")) {
            new_mesh_path = "data/5cell.mesh4";
        }
        if (ImGui::Button("Tesseract")) {
            new_mesh_path = "data/tesseract.mesh4";
        }
        if (ImGui::Button("16-cell")) {
            new_mesh_path = "data/16cell.mesh4";
        }
        if (ImGui::Button("24-cell")) {
            new_mesh_path = "data/24cell.mesh4";
        }
        if (ImGui::Button("120-cell")) {
            new_mesh_path = "data/120cell.mesh4";
        }
        if (ImGui::Button("600-cell")) {
            new_mesh_path = "data/600cell.mesh4";
        }

        ImGui::End();
        if (new_mesh_path) {
            change_mesh(new_mesh_path);
        }
    }

    // Selected cell window
    {
        ImGui::Begin("Selected cell", NULL, ImGuiWindowFlags_NoScrollbar);
        ImGui::Checkbox("Cycle", &selected_cell_cycle);
        ImVec2 cycle_size = ImGui::GetItemRectSize();

        ImGui::PushItemWidth(-1);
        auto list_box_size =
                ImVec2(0, ImGui::GetWindowHeight() - 1.7f * ImGui::GetWindowContentRegionMin().y - cycle_size.y);
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
        ImGui::End();
    }

    {
        ImGui::Begin("4D Transform");
        const f32 speed = 0.01f;
        const auto fmt = "%.3f";

        ImGui::Text("Translate");
        imgui_drag_f64("x##t", &mesh_pos.X, speed, fmt);
        imgui_drag_f64("y##t", &mesh_pos.Y, speed, fmt);
        imgui_drag_f64("z##t", &mesh_pos.Z, speed, fmt);
        imgui_drag_f64("w##t", &mesh_pos.W, speed, fmt);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Scale");

        {
            f64 scale_magnitude =
                    (std::abs(mesh_scale.X) + std::abs(mesh_scale.Y) + std::abs(mesh_scale.Z) + std::abs(mesh_scale.W))
                    / 4.0;
            f64 scale_speed = speed * scale_magnitude;
            ImGui::PushButtonRepeat(true);
            if (ImGui::Button("−")) {
                mesh_scale.X -= scale_speed;
                mesh_scale.Y -= scale_speed;
                mesh_scale.Z -= scale_speed;
                mesh_scale.W -= scale_speed;
            }
            ImVec2 minus_size = ImGui::GetItemRectSize();
            ImGui::SameLine();
            if (ImGui::Button("+", minus_size)) {
                mesh_scale.X += scale_speed;
                mesh_scale.Y += scale_speed;
                mesh_scale.Z += scale_speed;
                mesh_scale.W += scale_speed;
            }
            ImGui::PopButtonRepeat();
            ImGui::SameLine();
            ImGui::Text("xyzw");
        }

        imgui_drag_f64("x##s", &mesh_scale.X, speed, fmt);
        imgui_drag_f64("y##s", &mesh_scale.Y, speed, fmt);
        imgui_drag_f64("z##s", &mesh_scale.Z, speed, fmt);
        imgui_drag_f64("w##s", &mesh_scale.W, speed, fmt);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Rotate");

        imgui_drag_f64("xy", &mesh_rotation.xy, speed, fmt);
        imgui_drag_f64("xz", &mesh_rotation.xz, speed, fmt);
        imgui_drag_f64("xw", &mesh_rotation.xw, speed, fmt);
        imgui_drag_f64("yz", &mesh_rotation.yz, speed, fmt);
        imgui_drag_f64("yw", &mesh_rotation.yw, speed, fmt);
        imgui_drag_f64("zw", &mesh_rotation.zw, speed, fmt);

        ImGui::End();
    }

    ImGui::EndFrame();
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
} // namespace four
