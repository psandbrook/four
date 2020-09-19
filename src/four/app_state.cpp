#include <four/app_state.hpp>

#include <four/math.hpp>
#include <four/resource.hpp>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <loguru.hpp>

#include <SDL.h>

namespace four {

namespace {

constexpr f64 mouse_motion_fac = 0.002;
const ImWchar glyph_ranges[] = {0x20, 0xFFFF, 0};

const char* plane_str[] = {
        "xy", "xz", "xw", "yz", "yw", "zw",
};

const char* mesh_paths[] = {
        "5-cell.mesh4", "Tesseract.mesh4", "16-cell.mesh4", "24-cell.mesh4", "120-cell.mesh4", "600-cell.mesh4",
};

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

bool is_around(f64 target, f64 pos) {
    constexpr f64 tolerance = 0.003;
    return pos >= target - tolerance && pos <= target + tolerance;
}
} // namespace

AppState::AppState(SDL_Window* window, ImGuiIO* imgui_io)
        : window(window), imgui_io(imgui_io), random_dev(), random_eng_32(random_dev()) {

    SDL_GL_GetDrawableSize(window, &window_width, &window_height);
    calc_ui_size_screen();
    imgui_io->IniFilename = NULL;
    CHECK_NOTNULL_F(imgui_io->Fonts->AddFontFromFileTTF(get_resource_path("DejaVuSans.ttf").c_str(), 18.0f, NULL,
                                                        glyph_ranges));

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

    meshes.push_back(Mesh4{});

    for (const char* path : mesh_paths) {
        auto res_path = std::string("meshes/") + path;
        Mesh4 mesh = load_mesh_from_file(get_resource_path(res_path.c_str()).c_str());
        meshes.push_back(std::move(mesh));
    }

    n5cell_index = mesh_with_name("5-cell");
    tesseract_index = mesh_with_name("Tesseract");
    n16cell_index = mesh_with_name("16-cell");
    n24cell_index = mesh_with_name("24-cell");
    n120cell_index = mesh_with_name("120-cell");
    n600cell_index = mesh_with_name("600-cell");

    add_mesh_instance(tesseract_index);
}

f64 AppState::screen_x(f64 x) {
    return x * window_width;
}

f64 AppState::screen_y(f64 y) {
    return y * window_height;
}

f64 AppState::norm_x(f64 x) {
    return x / window_width;
}

bool AppState::is_mouse_around_x(f64 x) {
    return is_around(x, norm_x(ImGui::GetMousePos().x));
}

u32 AppState::mesh_with_name(const char* name) {
    for (u32 i = 0; i < meshes.size(); i++) {
        if (meshes.at(i).name == name) {
            return i;
        }
    }
    ABORT_F("No mesh with name %s", name);
}

void AppState::add_mesh_instance(u32 mesh_index) {
    MeshInstance mesh_instance = {};
    mesh_instance.mesh_index = mesh_index;
    mesh_instance.transform.position = {0.0, 0.0, 0.0, 0.0};
    mesh_instance.transform.scale = {1, 1, 1, 1};
    mesh_instance.transform.rotation.is_rotor = false;
    mesh_instance.transform.rotation.euler = Bivec4{};
    mesh_instance.auto_rotate_magnitude.fill(0.01);

    while (has_key(mesh_instances, next_mesh_instance_id)) {
        next_mesh_instance_id++;
    }

    const u32 mesh_instance_id = next_mesh_instance_id;
    next_mesh_instance_id++;
    mesh_instances.emplace(mesh_instance_id, mesh_instance);
    mesh_instances_insertion.push_back(mesh_instance_id);

    mesh_instances_events.push_back({MeshInstancesEvent::Type::added, mesh_instance_id});
    set_selected_mesh_instance(mesh_instance_id);
}

void AppState::remove_mesh_instance(u32 mesh_instance) {
    mesh_instances.erase(mesh_instance);
    mesh_instances_insertion.erase(
            std::find(mesh_instances_insertion.begin(), mesh_instances_insertion.end(), mesh_instance));
    next_mesh_instance_id = std::min(next_mesh_instance_id, mesh_instance);
    mesh_instances_events.push_back({MeshInstancesEvent::Type::removed, mesh_instance});
}

void AppState::set_selected_mesh_instance(u32 mesh_instance) {
    selected_mesh_instance = mesh_instance;
    selected_cell = 0;
}

void AppState::calc_ui_size_screen() {
    ui_size_screen = window_width - screen_x(visualization_width);
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

                case SDLK_w: {
                    wireframe_render = !wireframe_render;
                } break;
                }
            }
        } break;

        case SDL_MOUSEMOTION: {
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {

                if (SDL_GetModState() & KMOD_SHIFT) {
                    // Pan the 3D camera

                    glm::dvec3 front = camera_target - camera_pos;
                    f64 distance_fac = 0.25 * glm::length(front);

                    glm::dvec3 f = glm::normalize(front);
                    glm::dvec3 left = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), f));
                    glm::dvec3 up = glm::cross(f, left);

                    f64 x_move = mouse_motion_fac * distance_fac * event.motion.xrel;
                    f64 y_move = mouse_motion_fac * distance_fac * event.motion.yrel;

                    glm::dmat4 y_trans = translate(y_move * up);
                    glm::dmat4 x_trans = translate(x_move * left);
                    glm::dmat4 translation = y_trans * x_trans;

                    camera_pos = transform(translation, camera_pos);
                    camera_target = transform(translation, camera_target);

                } else {
                    // Rotate the 3D camera

                    f64 x_angle = mouse_motion_fac * event.motion.xrel;
                    Rotor3 x_rotor = rotor3(x_angle, outer(glm::dvec3(0, 0, -1), glm::dvec3(1, 0, 0)));

                    f64 y_angle = mouse_motion_fac * event.motion.yrel;
                    Rotor3 y_rotor = rotor3(y_angle, outer(glm::dvec3(0, 1, 0), camera_target - camera_pos));

                    glm::dmat4 rotation = to_mat4(y_rotor * x_rotor);
                    glm::dmat4 m = translate(camera_target) * rotation * translate(-1.0 * camera_target);

                    glm::dvec3 new_camera_pos = transform(m, camera_pos);
                    glm::dvec3 front = glm::normalize(camera_target - new_camera_pos);
                    if (!float_eq(std::abs(front.y), 1.0, 0.001)) {
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

            glm::dvec3 front = camera_target - camera_pos;
            f64 distance_fac = 0.1 * glm::length(front);
            glm::dvec3 f = glm::normalize(front);
            glm::dmat4 translation = translate(y * distance_fac * f);
            glm::dvec3 new_camera_pos = transform(translation, camera_pos);
            if (!float_eq(new_camera_pos, camera_target)) {
                camera_pos = new_camera_pos;
            }

        } break;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    if (debug) {
        ImGui::ShowDemoWindow();
    }

    if (ImGui::IsMouseClicked(0)) {
        if (split && is_mouse_around_x(visualization_width * divider)) {
            dragging_divider = true;
        } else if (is_mouse_around_x(visualization_width)) {
            dragging_ui = true;
        }
    }

    if (ImGui::IsMouseDragging(0, 0.0f)) {
        if (dragging_divider) {
            f64 new_divider = divider + imgui_io->MouseDelta.x / (window_width * visualization_width);
            if (new_divider >= 0.0 && new_divider <= 1.0) {
                divider = new_divider;
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        } else if (dragging_ui) {
            f64 new_width = visualization_width + norm_x(imgui_io->MouseDelta.x);
            if (new_width >= 0.1 && new_width <= 0.9) {
                visualization_width = new_width;
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            window_size_changed = true;
        }

    } else {
        dragging_ui = false;
        dragging_divider = false;

        if (is_mouse_around_x(visualization_width) || (split && is_mouse_around_x(visualization_width * divider))) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }

    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
                                          | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
                                          | ImGuiWindowFlags_NoNav;

    calc_ui_size_screen();
    ImGui::SetNextWindowPos(ImVec2((f32)screen_x(visualization_width), 0));
    ImGui::SetNextWindowSize(ImVec2((f32)ui_size_screen, (f32)window_height));
    ImGui::SetNextWindowBgAlpha(0xff);
    ImGui::Begin("four", NULL, window_flags);

    auto& selected_mesh_instance_data = get_selected_mesh_instance();
    auto& mesh = meshes.at(selected_mesh_instance_data.mesh_index);
    auto& mesh_transform = selected_mesh_instance_data.transform;
    auto& auto_rotate = selected_mesh_instance_data.auto_rotate;
    auto& auto_rotate_mag = selected_mesh_instance_data.auto_rotate_magnitude;

    ImGui::BeginChild("ui_left", ImVec2(ImGui::GetContentRegionAvailWidth() * 0.57f, 0), true, window_flags);

    ImGui::Checkbox("Split", &split);

    ImGui::Spacing();
    ImGui::Separator();

    {
        constexpr f32 speed = 0.01f;
        const auto fmt = "%.3f";

        {
            ImGui::Text("4D Camera");
            const char* label = perspective_projection ? "Perspective###projection" : "Orthographic###projection";
            if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
                perspective_projection = !perspective_projection;
            }

            const Camera4 old_camera4 = camera4;
            imgui_drag_f64("w##camera", &camera4.pos.w, speed, fmt);

            if (float_eq(camera4.pos, camera4.target)) {
                camera4 = old_camera4;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text("Translate");
        imgui_drag_f64("x##t", &mesh_transform.position.x, speed, fmt);
        imgui_drag_f64("y##t", &mesh_transform.position.y, speed, fmt);
        imgui_drag_f64("z##t", &mesh_transform.position.z, speed, fmt);
        imgui_drag_f64("w##t", &mesh_transform.position.w, speed, fmt);

        ImGui::Spacing();
        ImGui::Separator();

        {
            ImGui::Text("Scale");

            ImGui::Button("xyzw##s", ImVec2(ImGui::GetContentRegionAvailWidth(), 0));
            if (ImGui::IsItemActive()) {
                f64 scale_magnitude = (std::abs(mesh_transform.scale.x) + std::abs(mesh_transform.scale.y)
                                       + std::abs(mesh_transform.scale.z) + std::abs(mesh_transform.scale.w))
                                      / 4.0;
                f64 scale_speed = speed * scale_magnitude;

                f64 xyzw_scale = scale_speed * imgui_io->MouseDelta.x;
                mesh_transform.scale.x += xyzw_scale;
                mesh_transform.scale.y += xyzw_scale;
                mesh_transform.scale.z += xyzw_scale;
                mesh_transform.scale.w += xyzw_scale;
            }

            imgui_drag_f64("x##s", &mesh_transform.scale.x, speed, fmt);
            imgui_drag_f64("y##s", &mesh_transform.scale.y, speed, fmt);
            imgui_drag_f64("z##s", &mesh_transform.scale.z, speed, fmt);
            imgui_drag_f64("w##s", &mesh_transform.scale.w, speed, fmt);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Rotate");

        if (ImGui::BeginTabBar("RotationType")) {
            if (ImGui::BeginTabItem("Euler")) {
                if (mesh_transform.rotation.is_rotor) {
                    mesh_transform.rotation.is_rotor = false;
                    mesh_transform.rotation.euler = rotor_to_euler(mesh_transform.rotation.rotor);
                }

                imgui_drag_f64("xy", &mesh_transform.rotation.euler.xy, speed, fmt);
                imgui_drag_f64("xz", &mesh_transform.rotation.euler.xz, speed, fmt);
                imgui_drag_f64("xw", &mesh_transform.rotation.euler.xw, speed, fmt);
                imgui_drag_f64("yz", &mesh_transform.rotation.euler.yz, speed, fmt);
                imgui_drag_f64("yw", &mesh_transform.rotation.euler.yw, speed, fmt);
                imgui_drag_f64("zw", &mesh_transform.rotation.euler.zw, speed, fmt);

                ImGui::Spacing();
                ImGui::Text("Auto rotate");

                {
                    constexpr f32 speed = 0.0001f;
                    const auto fmt = "%.4f";
                    for (size_t i = 0; i < plane4_n; i++) {
                        char id_label[5] = "##";
                        strncpy(&id_label[2], plane_str[i], 3);
                        ImGui::Checkbox(id_label, &auto_rotate[i]);
                        ImGui::SameLine(0.0f, 2.0f);

                        char drag_label[6] = "__##a";
                        strncpy(drag_label, plane_str[i], 2);
                        imgui_drag_f64(drag_label, &auto_rotate_mag[i], speed, fmt);
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Rotor")) {
                if (!mesh_transform.rotation.is_rotor) {
                    mesh_transform.rotation.is_rotor = true;
                    mesh_transform.rotation.rotor = euler_to_rotor(mesh_transform.rotation.euler);
                    for (auto& e : auto_rotate) {
                        e = false;
                    }
                }
                imgui_drag_f64("s", &mesh_transform.rotation.rotor.s, speed, fmt);
                imgui_drag_f64("xy", &mesh_transform.rotation.rotor.B.xy, speed, fmt);
                imgui_drag_f64("xz", &mesh_transform.rotation.rotor.B.xz, speed, fmt);
                imgui_drag_f64("xw", &mesh_transform.rotation.rotor.B.xw, speed, fmt);
                imgui_drag_f64("yz", &mesh_transform.rotation.rotor.B.yz, speed, fmt);
                imgui_drag_f64("yw", &mesh_transform.rotation.rotor.B.yw, speed, fmt);
                imgui_drag_f64("zw", &mesh_transform.rotation.rotor.B.zw, speed, fmt);
                imgui_drag_f64("xyzw", &mesh_transform.rotation.rotor.xyzw, speed, fmt);
                mesh_transform.rotation.rotor = normalize(mesh_transform.rotation.rotor);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("ui_right", ImVec2(0, 0), true, window_flags);

    {
        auto button_size = ImVec2(ImGui::GetContentRegionAvailWidth(), 0);

        ImGui::Text("Outliner");

        if (ImGui::Button("Delete", button_size)) {
            if (!mesh_instances.empty()) {

                u32 sel_insertion_i = 0;
                while (mesh_instances_insertion.at(sel_insertion_i) != selected_mesh_instance) {
                    sel_insertion_i++;
                }

                remove_mesh_instance(selected_mesh_instance);

                if (sel_insertion_i == mesh_instances_insertion.size()) {
                    sel_insertion_i--;
                }

                if (!mesh_instances.empty()) {
                    set_selected_mesh_instance(mesh_instances_insertion.at(sel_insertion_i));
                }
            }
        }

        ImGui::PushItemWidth(-1);
        const auto list_box_size = ImVec2(0, ImGui::GetWindowHeight() * 0.25f);

        if (ImGui::BeginChildFrame(ImGui::GetID("##outliner_list_box"), list_box_size,
                                   ImGuiWindowFlags_HorizontalScrollbar)) {
            for (u32 mesh_instance : mesh_instances_insertion) {
                const auto fmt_str = "%s [%u]";
                const auto& mesh = get_mesh(mesh_instance);

                s32 str_size = snprintf(NULL, 0, fmt_str, mesh.name.c_str(), mesh_instance);
                str_buffer.resize((size_t)str_size + 1);
                snprintf(str_buffer.data(), str_buffer.size(), fmt_str, mesh.name.c_str(), mesh_instance);

                if (ImGui::Selectable(str_buffer.data(), selected_mesh_instance == mesh_instance)) {
                    set_selected_mesh_instance(mesh_instance);
                }
            }
        }
        ImGui::EndChildFrame();
        ImGui::PopItemWidth();

        ImGui::Text("Add object");

        if (ImGui::Button("5-cell", button_size)) {
            add_mesh_instance(n5cell_index);
        }
        if (ImGui::Button("Tesseract", button_size)) {
            add_mesh_instance(tesseract_index);
        }
        if (ImGui::Button("16-cell", button_size)) {
            add_mesh_instance(n16cell_index);
        }
        if (ImGui::Button("24-cell", button_size)) {
            add_mesh_instance(n24cell_index);
        }
        if (ImGui::Button("120-cell", button_size)) {
            add_mesh_instance(n120cell_index);
        }
        if (ImGui::Button("600-cell", button_size)) {
            add_mesh_instance(n600cell_index);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    {
        ImGui::Text("Selected cell");
        ImGui::Checkbox("Show", &selected_cell_enabled);
        ImGui::Checkbox("Cycle", &selected_cell_cycle);

        ImGui::PushItemWidth(-1);
        const auto list_box_size = ImVec2(0, ImGui::GetContentRegionAvail().y);

        if (ImGui::ListBoxHeader("##selected_cell_empty", list_box_size)) {
            for (u32 i = 0; i < mesh.cells.size(); i++) {
                const auto fmt_str = "%u";
                s32 str_size = snprintf(NULL, 0, fmt_str, i);
                str_buffer.resize((size_t)str_size + 1);
                snprintf(str_buffer.data(), str_buffer.size(), fmt_str, i);

                if (ImGui::Selectable(str_buffer.data(), selected_cell == i)) {
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

    return false;
}

void AppState::step(const f64 ms) {

    if (!mesh_instances.empty()) {
        auto& mesh = get_mesh(selected_mesh_instance);

        if (selected_cell_cycle) {
            selected_cell_cycle_acc += ms;
            if (selected_cell_cycle_acc >= 2000.0) {
                selected_cell++;
                if (selected_cell == mesh.cells.size()) {
                    selected_cell = 0;
                }
                selected_cell_cycle_acc = 0.0;
            }
        } else {
            selected_cell_cycle_acc = 0.0;
        }
    }

    for (auto& [_, mesh_instance_data] : mesh_instances) {
        auto& mesh_transform = mesh_instance_data.transform;
        auto& auto_rotate = mesh_instance_data.auto_rotate;
        auto& auto_rotate_magnitude = mesh_instance_data.auto_rotate_magnitude;
        for (size_t plane4_i = 0; plane4_i < plane4_n; plane4_i++) {
            if (auto_rotate[plane4_i]) {
                mesh_transform.rotation.euler[plane4_i] += auto_rotate_magnitude[plane4_i];
            }
        }
    }
}

void AppState::bump_mesh_pos_w(u32 mesh_instance) {
    constexpr f64 mag = 0.0000001;
    auto& mesh_transform = get_transform(mesh_instance);
    mesh_transform.position.w += mag;
    LOG_F(WARNING, "New mesh instance %u w: %+.16f", mesh_instance, mesh_transform.position.w);
}

AppState::MeshInstance& AppState::get_selected_mesh_instance() {
    if (mesh_instances.empty()) {
        dummy_mesh_instance = AppState::MeshInstance{};
        return dummy_mesh_instance;
    } else {
        return mesh_instances.at(selected_mesh_instance);
    }
}

Mesh4& AppState::get_mesh(u32 mesh_instance) {
    return meshes.at(mesh_instances.at(mesh_instance).mesh_index);
}

Transform4& AppState::get_transform(u32 mesh_instance) {
    return mesh_instances.at(mesh_instance).transform;
}

Mat5 mk_model_mat(const Transform4& transform4) {
    Mat5 m_r;
    if (transform4.rotation.is_rotor) {
        m_r = to_mat5(transform4.rotation.rotor);
    } else {
        m_r = rotate_euler(transform4.rotation.euler);
    }

    return translate(transform4.position) * m_r * scale(transform4.scale);
}

Mat5 mk_model_view_mat(const Mat5& model, const Camera4& camera) {
    Mat5 view = look_at(camera.pos, camera.target, camera.up, camera.over);
    return view * model;
}
} // namespace four
