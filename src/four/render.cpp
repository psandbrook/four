#include <four/math.hpp>
#include <four/render.hpp>

#include <SDL.h>
#include <imgui_impl_opengl3.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace four {

namespace {

u32 compile_shader(const char* path, GLenum type) {

    // FIXME: This is inefficient
    std::ifstream stream(path);
    CHECK_F(bool(stream), "Could not open file \"%s\"", path);

    std::stringstream source_buffer;
    source_buffer << stream.rdbuf();
    std::string source = source_buffer.str();
    const char* source_c = source.c_str();

    u32 shader = glCreateShader(type);
    glShaderSource(shader, 1, &source_c, NULL);
    glCompileShader(shader);

    s32 success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        s32 len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log((size_t)len);
        glGetShaderInfoLog(shader, len, NULL, log.data());
        ABORT_F("Shader compilation failed: %s", log.data());
    }

    return shader;
}
} // namespace

ShaderProgram::ShaderProgram(u32 vertex_shader, u32 fragment_shader) {
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);

    s32 success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        s32 len;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log((size_t)len);
        glGetProgramInfoLog(id, len, NULL, log.data());
        ABORT_F("Program linking failed: %s", log.data());
    }
}

s32 ShaderProgram::get_location(const char* name) {
    auto it = uniform_locations.find(name);
    s32 location;
    if (it == uniform_locations.end()) {
        location = glGetUniformLocation(id, name);
        CHECK_NE_F(location, -1);
        uniform_locations.emplace(name, location);
    } else {
        location = it->second;
    }

    return location;
}

void ShaderProgram::set_uniform_mat4(const char* name, const f32* data) {
    glUseProgram(id);
    glUniformMatrix4fv(get_location(name), 1, false, data);
}

void ShaderProgram::set_uniform_f32(const char* name, f32 value) {
    glUseProgram(id);
    glUniform1f(get_location(name), value);
}

GlBuffer::GlBuffer(GLenum type, GLenum usage) : type(type), usage(usage) {
    glGenBuffers(1, &id);
}

void GlBuffer::buffer_data(const void* data, size_t size) {
    glBindBuffer(type, id);
    if (this->size < size) {
        glBufferData(type, size, data, usage);
        this->size = size;
    } else {
        CHECK_NE_F(usage, (u32)GL_STATIC_DRAW);
        glBufferSubData(type, 0, size, data);
    }
}

void GlBuffer::buffer_data_realloc(const void* data, size_t size) {
    glBindBuffer(type, id);
    glBufferData(type, size, data, usage);
    this->size = size;
}

void ElementBufferObject::buffer_data(const void* data, s32 n) {
    buf.buffer_data(data, (u32)n * sizeof(u32));
    primitive_count = n;
}

void ElementBufferObject::buffer_data_realloc(const void* data, s32 n) {
    buf.buffer_data_realloc(data, (u32)n * sizeof(u32));
    primitive_count = n;
}

VertexArrayObject::VertexArrayObject(ShaderProgram* shader_program, Slice<VertexBufferObject*> vbos_,
                                     Slice<VertexSpec> specs, ElementBufferObject ebo)
        : shader_program(shader_program), vbos(vbos_.data, vbos_.data + vbos_.len), ebo(ebo) {

    for (auto vbo : vbos) {
        CHECK_EQ_F(vbo->type, (u32)GL_ARRAY_BUFFER);
    }

    CHECK_EQ_F(ebo.buf.type, (u32)GL_ELEMENT_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    for (size_t i = 0; i < vbos.size(); i++) {
        auto vbo = vbos[i];
        VertexSpec spec = specs[i];
        glBindBuffer(vbo->type, vbo->id);
        glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride, (void*)spec.offset);
        glEnableVertexAttribArray(spec.index);
    }

    glBindBuffer(ebo.buf.type, ebo.buf.id);

    glBindVertexArray(0);
}

void VertexArrayObject::draw() {
    glUseProgram(shader_program->id);
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.primitive_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

size_t Renderer::add_vbo(GLenum usage) {
    return insert_back(vbos, new_vertex_buffer_object(usage));
}

void Renderer::update_window_size() {
    s32 window_width, window_height;
    SDL_GL_GetDrawableSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);
    projection = HMM_Perspective(90, (f64)window_width / (f64)window_height, 0.1, 100.0);
}

void Renderer::do_mesh_changed() {
    auto& s = *state;
    wireframe.ebo.buffer_data_realloc(s.mesh.edges.data(), 2 * (s32)s.mesh.edges.size());

    tet_mesh_vertices.clear();
    tet_mesh_tets.clear();

    for (const Cell& cell : s.mesh.cells) {
        out_tets.clear();
        render_funcs.tetrahedralize(s.mesh.vertices, s.mesh.edges, s.mesh.faces, cell, tet_mesh_vertices, out_tets);
        CHECK_EQ_F((s64)out_tets.size() % 4, 0);

        f32 color[3] = {color_dist(s.random_eng_32), color_dist(s.random_eng_32), color_dist(s.random_eng_32)};
        for (size_t tet_i = 0; tet_i < out_tets.size() / 4; tet_i++) {
            Tet tet;
            tet.color[0] = color[0];
            tet.color[1] = color[1];
            tet.color[2] = color[2];
            for (size_t j = 0; j < 4; j++) {
                tet.vertices[j] = out_tets[tet_i * 4 + j];
            }
            tet_mesh_tets.push_back(tet);
        }
    }
}

Renderer::Renderer(SDL_Window* window, AppState* state)
        : window(window), state(state), color_dist(0.0f, std::nextafter(1.0f, std::numeric_limits<f32>::max())) {
    auto& s = *state;

    SDL_GL_SetSwapInterval(1);

    update_window_size();

    glClearColor(0.23f, 0.23f, 0.23f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0, -1.0);

    // Wireframe
    // ---------

    u32 n4d_vert_shader = compile_shader("data/n4d-vert.glsl", GL_VERTEX_SHADER);
    u32 wireframe_frag_shader = compile_shader("data/wireframe-frag.glsl", GL_FRAGMENT_SHADER);
    wireframe_shader_prog = ShaderProgram(n4d_vert_shader, wireframe_frag_shader);

    size_t wireframe_vertices = add_vbo(GL_STREAM_DRAW);
    VertexSpec wireframe_vertices_spec = {
            .index = 0,
            .size = 4,
            .type = GL_FLOAT,
            .stride = 4 * sizeof(f32),
            .offset = 0,
    };

    // ---------

    // Selected cell
    // -------------

    u32 selected_cell_frag_shader = compile_shader("data/cell-frag.glsl", GL_FRAGMENT_SHADER);
    selected_cell_shader_prog = ShaderProgram(n4d_vert_shader, selected_cell_frag_shader);

    // -------------

    // Cross-section
    // -------------

    u32 cross_vert_shader = compile_shader("data/cross-vert.glsl", GL_VERTEX_SHADER);
    u32 cross_frag_shader = compile_shader("data/cross-frag.glsl", GL_FRAGMENT_SHADER);
    cross_section_shader_prog = ShaderProgram(cross_vert_shader, cross_frag_shader);
    size_t cross_vertices = add_vbo(GL_STREAM_DRAW);
    size_t cross_colors = add_vbo(GL_STREAM_DRAW);

    VertexSpec cross_spec = {
            .index = 0,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    VertexSpec cross_color_spec = {
            .index = 1,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    // -------------

    // XZ grid
    // -------

    u32 xz_grid_vert_shader = compile_shader("data/xz-grid-vert.glsl", GL_VERTEX_SHADER);
    u32 xz_grid_frag_shader = compile_shader("data/xz-grid-frag.glsl", GL_FRAGMENT_SHADER);
    xz_grid_shader_prog = ShaderProgram(xz_grid_vert_shader, xz_grid_frag_shader);
    size_t xz_grid_vertices = add_vbo(GL_STATIC_DRAW);
    std::vector<f32> xz_grid_vertices_vec;
    const s32 n_grid_lines = 20;
    const f64 grid_lines_spacing = 0.2;

    for (s32 i = 0; i <= n_grid_lines; i++) {
        f64 end_pos = (n_grid_lines / 2.0) * grid_lines_spacing;
        f64 i_pos = (i * grid_lines_spacing) - end_pos;
        f64 vertices[][3] = {
                {end_pos, 0, i_pos},
                {-end_pos, 0, i_pos},
                {i_pos, 0, -end_pos},
                {i_pos, 0, end_pos},
        };

        for (auto v : vertices) {
            for (s32 i = 0; i < 3; i++) {
                xz_grid_vertices_vec.push_back((f32)v[i]);
            }
        }
    }
    vbos[xz_grid_vertices].buffer_data(xz_grid_vertices_vec.data(), xz_grid_vertices_vec.size() * sizeof(f32));

    // -------

    // Wireframe VAO
    // -------------

    ElementBufferObject wireframe_ebo(GL_STATIC_DRAW, GL_LINES);

    VertexBufferObject* wireframe_vbos[] = {&vbos[wireframe_vertices]};
    VertexSpec wireframe_vertex_specs[] = {wireframe_vertices_spec};
    wireframe = VertexArrayObject(&wireframe_shader_prog, AS_SLICE(wireframe_vbos), AS_SLICE(wireframe_vertex_specs),
                                  wireframe_ebo);
    // -------------

    // Selected cell VAO
    // -----------------

    ElementBufferObject cell_ebo(GL_STREAM_DRAW, GL_TRIANGLES);

    VertexBufferObject* selected_cell_vbos[] = {&vbos[wireframe_vertices]};
    selected_cell = VertexArrayObject(&selected_cell_shader_prog, AS_SLICE(selected_cell_vbos),
                                      AS_SLICE(wireframe_vertex_specs), cell_ebo);

    // -----------------

    // Cross-section VAO
    // -----------------

    ElementBufferObject cross_ebo(GL_STREAM_DRAW, GL_TRIANGLES);

    VertexBufferObject* cross_vbos[] = {&vbos[cross_vertices], &vbos[cross_colors]};
    VertexSpec cross_vertex_specs[] = {cross_spec, cross_color_spec};
    cross_section = VertexArrayObject(&cross_section_shader_prog, AS_SLICE(cross_vbos), AS_SLICE(cross_vertex_specs),
                                      cross_ebo);

    // -----------------

    // XZ grid VAO
    // -----------

    ElementBufferObject xz_grid_ebo(GL_STATIC_DRAW, GL_LINES);
    std::vector<u32> xz_grid_indices_vec;
    for (u32 i = 0; i < xz_grid_vertices_vec.size(); i++) {
        xz_grid_indices_vec.push_back(i);
    }
    xz_grid_ebo.buffer_data(xz_grid_indices_vec.data(), (s32)xz_grid_indices_vec.size());

    VertexBufferObject* xz_grid_vbos[] = {&vbos[xz_grid_vertices]};
    VertexSpec xz_grid_vertex_specs[] = {cross_spec};
    xz_grid = VertexArrayObject(&xz_grid_shader_prog, AS_SLICE(xz_grid_vbos), AS_SLICE(xz_grid_vertex_specs),
                                xz_grid_ebo);

    // -----------

    do_mesh_changed();
}

void Renderer::render() {
    auto& s = *state;

    if (s.window_size_changed) {
        s.window_size_changed = false;
        update_window_size();
    }

    if (s.mesh_changed) {
        s.mesh_changed = false;
        do_mesh_changed();
    }

    // Cross-section
    // Hyperplane is p_0 = (0, 0, 0, 0), n = (0, 0, 0, 1)
    cross_vertices.clear();
    cross_colors.clear();
    cross_tris.clear();
    {
        struct Intersect {
            enum Type { none = 0, point, line } type;
            union {
                hmm_vec3 point_value;
                u32 edge_index;
            };
        };

        const f64 epsilon = 0.00000000000001;
        const hmm_vec4 p_0 = HMM_Vec4(0, 0, 0, 0);
        const hmm_vec4 n = HMM_Vec4(0, 0, 0, 1);

        tet_mesh_vertices_world.clear();
        Mat5 model = mk_model_mat(s.mesh_pos, s.mesh_scale, s.mesh_rotation);
        for (const auto& v : tet_mesh_vertices) {
            tet_mesh_vertices_world.push_back(vec4(model * vec5(v, 1)));
        }

        for (const Tet& tet : tet_mesh_tets) {
            // clang-format off
            Edge edges[6] = {
                {tet.vertices[0], tet.vertices[1]},
                {tet.vertices[0], tet.vertices[2]},
                {tet.vertices[0], tet.vertices[3]},
                {tet.vertices[1], tet.vertices[2]},
                {tet.vertices[1], tet.vertices[3]},
                {tet.vertices[2], tet.vertices[3]},
            };
            // clang-format on

            s32 intersect_len = 0;
            bool intersect_points = false;
            Intersect intersect[6];
            memset(intersect, 0, sizeof(intersect));

            for (u32 i = 0; i < 6; i++) {
                const Edge& e = edges[i];
                hmm_vec4 l_0 = tet_mesh_vertices_world[e.v0];
                hmm_vec4 l = tet_mesh_vertices_world[e.v1] - l_0;
                if (float_eq(HMM_Dot(l, n), 0.0)) {
                    if (float_eq(HMM_Dot(p_0 - l_0, n), 0.0)) {
                        // Edge is within plane
                        DCHECK_F(!intersect_points);
                        intersect[intersect_len].type = Intersect::line;
                        intersect[intersect_len].edge_index = i;
                        intersect_len++;
                    }
                } else {
                    f64 d = HMM_Dot(p_0 - l_0, n) / HMM_Dot(l, n);
                    if (d > 0.0 && d < 1.0 && !float_eq(d, 0.0) && !float_eq(d, 1.0)) {
                        // Edge intersects with plane at a point
                        intersect_points = true;
                        intersect[intersect_len].type = Intersect::point;
                        hmm_vec4 point = d * l + l_0;
                        DCHECK_F(float_eq(point.W, 0.0, epsilon));
                        intersect[intersect_len].point_value = vec3(point);
                        intersect_len++;
                    }
                }
            }

            if (intersect_len > 0) {
                if (intersect_points) {
                    if (intersect_len == 3) {
                        for (s32 i = 0; i < 3; i++) {
                            DCHECK_EQ_F(intersect[i].type, Intersect::point);
                            DCHECK_EQ_F((s64)cross_vertices.size() % 3, 0);
                            cross_tris.push_back((u32)(cross_vertices.size() / 3));
                            for (f64 e : intersect[i].point_value.Elements) {
                                cross_vertices.push_back((f32)e);
                            }
                            for (f32 e : tet.color) {
                                cross_colors.push_back(e);
                            }
                        }

                    } else if (intersect_len == 4) {
                        hmm_vec3 p0 = intersect[0].point_value;
                        hmm_vec3 p1 = intersect[1].point_value;
                        hmm_vec3 p2 = intersect[2].point_value;
                        hmm_vec3 p3 = intersect[3].point_value;
                        u32 p_mapping[4];

                        for (s32 i = 0; i < 4; i++) {
                            DCHECK_EQ_F(intersect[i].type, Intersect::point);
                            DCHECK_EQ_F((s64)cross_vertices.size() % 3, 0);
                            p_mapping[i] = (u32)(cross_vertices.size() / 3);
                            for (f64 e : intersect[i].point_value.Elements) {
                                cross_vertices.push_back((f32)e);
                            }
                            for (f32 e : tet.color) {
                                cross_colors.push_back(e);
                            }
                        }

                        hmm_vec3 l0 = p1 - p0;
                        hmm_vec3 l1 = p2 - p0;
                        hmm_vec3 l2 = p3 - p0;
                        DCHECK_F(float_eq(HMM_Dot(HMM_Cross(l0, l1), l2), 0.0, epsilon));

                        f64 sum0 = HMM_LengthSquared(p1 - p0) + HMM_LengthSquared(p3 - p2);
                        f64 sum1 = HMM_LengthSquared(p2 - p0) + HMM_LengthSquared(p3 - p1);
                        f64 sum2 = HMM_LengthSquared(p3 - p0) + HMM_LengthSquared(p2 - p1);
                        if (sum0 > sum1 && sum0 > sum2) {
                            // p0 p1 is a diagonal
                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[1]);
                            cross_tris.push_back(p_mapping[2]);

                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[1]);
                            cross_tris.push_back(p_mapping[3]);
                        } else if (sum1 > sum0 && sum1 > sum2) {
                            // p0 p2 is a diagonal
                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[2]);
                            cross_tris.push_back(p_mapping[1]);

                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[2]);
                            cross_tris.push_back(p_mapping[3]);
                        } else {
                            // p0 p3 is a diagonal
                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[3]);
                            cross_tris.push_back(p_mapping[1]);

                            cross_tris.push_back(p_mapping[0]);
                            cross_tris.push_back(p_mapping[3]);
                            cross_tris.push_back(p_mapping[2]);
                        }

                    } else {
                        ABORT_F("Invalid intersections");
                    }

                } else {
                    LOG_F(INFO, "lines of intersection");
                    DCHECK_F(intersect_len == 3 || intersect_len == 6);
                    for (s32 i = 0; i < intersect_len; i++) {
                        DCHECK_EQ_F(intersect[i].type, Intersect::line);
                    }

                    if (intersect_len == 3) {
                        const Edge& e0 = edges[intersect[0].edge_index];
                        const Edge& e1 = edges[intersect[1].edge_index];
                        const Edge& e2 = edges[intersect[2].edge_index];
                        u32 v_indices[3];
                        v_indices[0] = e0.v0;
                        v_indices[1] = e1.v0 == v_indices[0] ? e1.v1 : e1.v0;
                        v_indices[2] = e2.v0 == v_indices[0] || e2.v0 == v_indices[1] ? e2.v1 : e2.v0;
                        for (u32 i : v_indices) {
                            cross_tris.push_back((u32)(cross_vertices.size() / 3));
                            hmm_vec3 v = vec3(tet_mesh_vertices_world[i]);
                            for (f64 e : v.Elements) {
                                cross_vertices.push_back((f32)e);
                            }
                            for (f32 e : tet.color) {
                                cross_colors.push_back(e);
                            }
                        }
                    } else if (intersect_len == 6) {
                        u32 v_mapping[4];
                        for (s32 i = 0; i < 4; i++) {
                            v_mapping[i] = (u32)(cross_vertices.size() / 3);
                            hmm_vec3 v = vec3(tet_mesh_vertices_world[tet.vertices[i]]);
                            for (f64 e : v.Elements) {
                                cross_vertices.push_back((f32)e);
                            }
                            for (f32 e : tet.color) {
                                cross_colors.push_back(e);
                            }
                        }

                        // clang-format off
                        u32 v_indices[12] = {
                            0, 1, 2,
                            0, 1, 3,
                            0, 2, 3,
                            1, 2, 3,
                        };
                        // clang-format on
                        for (u32 e : v_indices) {
                            cross_tris.push_back(v_mapping[e]);
                        }
                    }
                }
            }
        }
    }

#if 0
    // Perform 4D to 3D projection
    {
        projected_vertices.clear();
        projected_vertices3.clear();

        Mat5 mv = mk_model_view_mat(s.mesh_pos, s.mesh_scale, s.mesh_rotation, s.camera4);
        for (const hmm_vec4& v : s.mesh.vertices) {
            Vec5 view_v = mv * vec5(v, 1);
            hmm_vec4 v_ = project_perspective(view_v, s.camera4.near);
            projected_vertices.push_back(v_);
            projected_vertices3.push_back(vec3(v_));
        }
        DCHECK_EQ_F(s.mesh.vertices.size(), projected_vertices.size());
    }

    // Triangulate selected cell
    selected_cell_tri_faces.clear();
    for (u32 face_i : s.mesh.cells[(size_t)s.selected_cell]) {
        const auto& face = s.mesh.faces[face_i];
        render_funcs.triangulate(projected_vertices3, s.mesh.edges, face, selected_cell_tri_faces);
    }
#endif

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    CHECK_EQ_F(cross_vertices.size(), cross_colors.size());
    cross_section.vbos[0]->buffer_data(cross_vertices.data(), cross_vertices.size() * sizeof(f32));
    cross_section.vbos[1]->buffer_data(cross_colors.data(), cross_colors.size() * sizeof(f32));
    cross_section.ebo.buffer_data(cross_tris.data(), (s32)cross_tris.size());

#if 0
    f32 max_depth = 0.0f;
    projected_vertices_f32.clear();
    for (const hmm_vec4& v : projected_vertices) {
        if (v.W > max_depth) {
            max_depth = (f32)v.W;
        }
        for (f64 element : v.Elements) {
            projected_vertices_f32.push_back((f32)element);
        }
    }

    //wireframe.vbos[0]->buffer_data(projected_vertices_f32.data(), projected_vertices_f32.size() * 3 * sizeof(f32));
    wireframe.vbos[0]->buffer_data(projected_vertices_f32.data(), projected_vertices_f32.size() * sizeof(f32));
    selected_cell.ebo.buffer_data(selected_cell_tri_faces.data(), (s32)selected_cell_tri_faces.size());
#endif

    hmm_mat4 view = HMM_LookAt(s.camera_pos, s.camera_target, s.camera_up);
    hmm_mat4 vp = projection * view;

    f32 vp_f32[16];
    for (s32 col = 0; col < 4; col++) {
        for (s32 row = 0; row < 4; row++) {
            vp_f32[col * 4 + row] = (f32)vp[col][row];
        }
    }

#if 0
    wireframe_shader_prog.set_uniform_mat4("vp", vp_f32);
    wireframe_shader_prog.set_uniform_f32("max_depth", max_depth);

    selected_cell_shader_prog.set_uniform_mat4("vp", vp_f32);
    selected_cell_shader_prog.set_uniform_f32("max_depth", max_depth);
#endif

    cross_section_shader_prog.set_uniform_mat4("vp", vp_f32);
    xz_grid_shader_prog.set_uniform_mat4("vp", vp_f32);

    glLineWidth(0.5f);
    xz_grid.draw();

    cross_section.draw();

#if 0
    selected_cell.draw();

    glLineWidth(2.0f);
    wireframe.draw();
#endif

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}
} // namespace four
