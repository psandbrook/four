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

}

AppState::AppState(SDL_Window* window, ImGuiIO* imgui_io, const char* mesh_path) : window(window), imgui_io(imgui_io) {
    mesh = load_mesh_from_file(mesh_path);
    imgui_io->Fonts->AddFontFromFileTTF("data/DejaVuSans.ttf", 18.0f);
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
            mesh = load_mesh_from_file(new_mesh_path);
            selected_cell = 0;
            mesh_changed = true;
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

    Rotor4 r = rotor4({1, 0, 0, 0}, {1, 0, 0, 0.002});

    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i] = rotate(r, mesh.vertices[i]);
    }
}
} // namespace four
