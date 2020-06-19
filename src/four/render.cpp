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

const f64 divider_width = 0.007;

void mat4_to_f32(const hmm_mat4& mat, f32* out) {
    for (s32 col = 0; col < 4; col++) {
        for (s32 row = 0; row < 4; row++) {
            out[col * 4 + row] = (f32)mat[col][row];
        }
    }
}

Renderer* renderer = NULL;

VertexBufferObject& global_get_vbo(u32 index) {
    return renderer->vbos.at(index);
}

u32 compile_shader(const char* path, GLenum type) {
    auto full_path = std::string("data/shaders/") + path;
    std::ifstream stream(full_path);
    CHECK_F(bool(stream), "Could not open file \"%s\"", full_path.c_str());

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

void bind_default_framebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace

namespace gl_buffer_base {
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
} // namespace gl_buffer_base

ShaderProgram::ShaderProgram(u32 vertex_shader, Slice<u32> fragment_shaders) {
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);

    for (size_t i = 0; i < fragment_shaders.len; i++) {
        glAttachShader(id, fragment_shaders[i]);
    }

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

void ShaderProgram::set_uniform_bool(const char* name, bool value) {
    glUseProgram(id);
    glUniform1i(get_location(name), (s32)value);
}

void ShaderProgram::set_uniform_vec3(const char* name, const f32* data) {
    glUseProgram(id);
    glUniform3fv(get_location(name), 1, data);
}

void ShaderProgram::bind_uniform_block(const UniformBufferObject& ubo) {
    u32 index = glGetUniformBlockIndex(id, ubo.name);
    glUniformBlockBinding(id, index, ubo.binding);
}

void ElementBufferObject::buffer_elements(const void* data, s32 n) {
    buffer_data(data, (u32)n * sizeof(u32));
    primitive_count = n;
}

void ElementBufferObject::buffer_elements_realloc(const void* data, s32 n) {
    buffer_data_realloc(data, (u32)n * sizeof(u32));
    primitive_count = n;
}

UniformBufferObject::UniformBufferObject(const char* name, u32 binding, GLenum usage)
        : GlBuffer(GL_UNIFORM_BUFFER, usage), name(name), binding(binding) {

    glBindBufferBase(GL_UNIFORM_BUFFER, binding, id);
}

Framebuffer::Framebuffer(u32 width, u32 height) : width(width), height(height) {
    glGenFramebuffers(1, &id);
    bind();

    const u32 samples = 8;

    glGenRenderbuffers(2, &color_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, color_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rbo);

    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);

#ifdef FOUR_DEBUG
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
        break;

    case GL_FRAMEBUFFER_UNDEFINED:
        ABORT_F("GL_FRAMEBUFFER_UNDEFINED");

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");

    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");

    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");

    case GL_FRAMEBUFFER_UNSUPPORTED:
        ABORT_F("GL_FRAMEBUFFER_UNSUPPORTED");

    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");

    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS");

    default:
        ABORT_F("glCheckFramebufferStatus() failed");
    }
#endif

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    bind_default_framebuffer();
}

void Framebuffer::destroy() {
    glDeleteFramebuffers(1, &id);
    glDeleteRenderbuffers(2, &color_rbo);
    id = 0;
    color_rbo = 0;
    depth_rbo = 0;
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

VertexArrayObject::VertexArrayObject(ShaderProgram* shader_program, Slice<u32> vbos_, Slice<VertexSpec> specs,
                                     ElementBufferObject ebo)
        : shader_program(shader_program), vbos(vbos_.data, vbos_.data + vbos_.len), ebo(ebo) {

    for (size_t i = 0; i < vbos.size(); i++) {
        auto& vbo = get_vbo(i);
        CHECK_EQ_F(vbo.type, (u32)GL_ARRAY_BUFFER);
    }

    CHECK_EQ_F(ebo.type, (u32)GL_ELEMENT_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    for (size_t i = 0; i < vbos.size(); i++) {
        auto& vbo = get_vbo(i);
        VertexSpec spec = specs[i];
        glBindBuffer(vbo.type, vbo.id);
        glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride, (void*)spec.offset);
        glEnableVertexAttribArray(spec.index);
    }

    glBindBuffer(ebo.type, ebo.id);

    glBindVertexArray(0);
}

VertexBufferObject& VertexArrayObject::get_vbo(size_t index) {
    return global_get_vbo(vbos.at(index));
}

void VertexArrayObject::draw() {
    glUseProgram(shader_program->id);
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.primitive_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Renderer::Renderer(SDL_Window* window, AppState* state)
        : window(window), state(state), color_dist(0.0f, std::nextafter(1.0f, std::numeric_limits<f32>::max())) {

    renderer = this;
    SDL_GL_SetSwapInterval(1);

    const f32 bg_shade = 0.04f;
    glClearColor(bg_shade, bg_shade, bg_shade, 1.0f);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0, -1.0);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    do_window_size_changed();

    view_projection_ubo = UniformBufferObject("ViewProjection", 0, GL_STREAM_DRAW);

    // Wireframe & selected cell
    {
        u32 vert_shader = compile_shader("n4d-vert.glsl", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("n4d-frag.glsl", GL_FRAGMENT_SHADER);
        u32 frag_shaders[] = {frag_shader};
        n4d_shader_prog = ShaderProgram(vert_shader, AS_SLICE(frag_shaders));

        u32 wireframe_vertices = add_vbo(GL_STREAM_DRAW);
        VertexSpec vertex_spec = {
                .index = 0,
                .size = 4,
                .type = GL_FLOAT,
                .stride = 4 * sizeof(f32),
                .offset = 0,
        };

        VertexSpec vertex_specs[] = {vertex_spec};
        u32 vbos[] = {wireframe_vertices};

        ElementBufferObject wireframe_ebo(GL_STATIC_DRAW, GL_LINES);
        wireframe = VertexArrayObject(&n4d_shader_prog, AS_SLICE(vbos), AS_SLICE(vertex_specs), wireframe_ebo);

        ElementBufferObject selected_cell_ebo(GL_STREAM_DRAW, GL_TRIANGLES);
        selected_cell = VertexArrayObject(&n4d_shader_prog, AS_SLICE(vbos), AS_SLICE(vertex_specs), selected_cell_ebo);
    }

    // Cross-section

    VertexSpec cross_vertex_spec = {
            .index = 0,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    {
        u32 vert_shader = compile_shader("cross-vert.glsl", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("cross-frag.glsl", GL_FRAGMENT_SHADER);
        u32 frag_shaders[] = {frag_shader};
        cross_section_shader_prog = ShaderProgram(vert_shader, AS_SLICE(frag_shaders));

        u32 vertices = add_vbo(GL_STREAM_DRAW);
        u32 colors = add_vbo(GL_STREAM_DRAW);

        VertexSpec color_spec = {
                .index = 1,
                .size = 3,
                .type = GL_FLOAT,
                .stride = 3 * sizeof(f32),
                .offset = 0,
        };

        ElementBufferObject ebo(GL_STREAM_DRAW, GL_TRIANGLES);
        u32 vbos[] = {vertices, colors};
        VertexSpec vertex_specs[] = {cross_vertex_spec, color_spec};
        cross_section = VertexArrayObject(&cross_section_shader_prog, AS_SLICE(vbos), AS_SLICE(vertex_specs), ebo);
    }

    // XZ grid
    {
        u32 vert_shader = compile_shader("xz-grid-vert.glsl", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("xz-grid-frag.glsl", GL_FRAGMENT_SHADER);

        u32 frag_shaders[] = {frag_shader};
        xz_grid_shader_prog = ShaderProgram(vert_shader, AS_SLICE(frag_shaders));

        u32 xz_grid_vertices_vbo = add_vbo(GL_STATIC_DRAW);
        std::vector<f32> grid_vertices;
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
                    grid_vertices.push_back((f32)v[i]);
                }
            }
        }

        global_get_vbo(xz_grid_vertices_vbo).buffer_data(grid_vertices.data(), grid_vertices.size() * sizeof(f32));

        ElementBufferObject ebo(GL_STATIC_DRAW, GL_LINES);
        std::vector<u32> indices_vec;
        for (u32 i = 0; i < grid_vertices.size(); i++) {
            indices_vec.push_back(i);
        }
        ebo.buffer_elements(indices_vec.data(), (s32)indices_vec.size());

        u32 vbos[] = {xz_grid_vertices_vbo};
        VertexSpec vertex_specs[] = {cross_vertex_spec};
        xz_grid = VertexArrayObject(&xz_grid_shader_prog, AS_SLICE(vbos), AS_SLICE(vertex_specs), ebo);
    }

    ShaderProgram* shader_progs[] = {&n4d_shader_prog, &cross_section_shader_prog, &xz_grid_shader_prog};
    for (auto prog : shader_progs) {
        prog->bind_uniform_block(view_projection_ubo);
    }

    // Divider bar
    {
        u32 vert_shader = compile_shader("divider-vert.glsl", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("divider-frag.glsl", GL_FRAGMENT_SHADER);
        u32 frag_shaders[] = {frag_shader};
        divider_bar_shader_prog = ShaderProgram(vert_shader, AS_SLICE(frag_shaders));

        u32 vertices_vbo = add_vbo(GL_STATIC_DRAW);

        // clang-format off
        f32 vertices[] = {
            0.0f,               -1.0f, 0.0f,
            0.0f,                1.0f, 0.0f,
            (f32)divider_width, -1.0f, 0.0f,
            (f32)divider_width,  1.0f, 0.0f,
        };

        u32 indices[] = {
            0, 1, 2,
            1, 2, 3,
        };
        // clang-format on

        global_get_vbo(vertices_vbo).buffer_data(vertices, sizeof(vertices));

        ElementBufferObject ebo(GL_STATIC_DRAW, GL_TRIANGLES);
        ebo.buffer_elements(indices, ARRAY_SIZE(indices));
        u32 vbos[] = {vertices_vbo};
        VertexSpec vertex_specs[] = {cross_vertex_spec};
        divider_bar = VertexArrayObject(&divider_bar_shader_prog, AS_SLICE(vbos), AS_SLICE(vertex_specs), ebo);
    }
}

u32 Renderer::add_vbo(GLenum usage) {
    return (u32)insert_back(vbos, VertexBufferObject(usage));
}

void Renderer::do_window_size_changed() {
    auto& s = *state;
    combined_buffer.destroy();
    projection_buffer.destroy();

    vis_width_screen = (u32)(s.window_width - s.screen_x(1.0 - s.visualization_width));
    combined_buffer = Framebuffer(vis_width_screen, (u32)s.window_height);
    projection_buffer = Framebuffer(vis_width_screen, (u32)s.window_height);
}

void Renderer::do_mesh_changed() {
    auto& s = *state;
    wireframe.ebo.buffer_elements_realloc(s.mesh.edges.data(), 2 * (s32)s.mesh.edges.size());

    tet_mesh_vertices.clear();
    tet_mesh_tets.clear();

    for (s64 i = 0; i < (s64)s.mesh.cells.size(); i++) {
        const Cell& cell = s.mesh.cells[(size_t)i];
        out_tets.clear();

        CHECK_GE_F(cell.size(), 4u);
        if (cell.size() == 4) {
            // The cell is already a tetrahedron

            size_t vertex_indices_size = 0;
            u32 vertex_indices[4];
            for (u32 f_i : cell) {
                const Face& f = s.mesh.faces[f_i];
                for (u32 e_i : f) {
                    const Edge& e = s.mesh.edges[e_i];
                    for (u32 v_i : e.vertices) {
                        if (!contains(Slice(vertex_indices_size, vertex_indices), v_i)) {
                            CHECK_LT_F(vertex_indices_size, 4u);
                            vertex_indices[vertex_indices_size] = v_i;
                            vertex_indices_size++;
                            out_tets.push_back((u32)tet_mesh_vertices.size());
                            tet_mesh_vertices.push_back(s.mesh.vertices[v_i]);
                        }
                    }
                }
            }

        } else {
            LOG_F(1, "Tetrahedralizing cell %li with %lu faces", i, cell.size());
            bool success = render_funcs.tetrahedralize(s.mesh.vertices, s.mesh.edges, s.mesh.faces, cell,
                                                       tet_mesh_vertices, out_tets);
            CHECK_F(success);
        }

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

void Renderer::calculate_cross_section() {
    auto& s = *state;

    const f64 epsilon = 0.0000001;

    const hmm_vec4 p_0 = HMM_Vec4(0, 0, 0, 0);
    const hmm_vec4 n = HMM_Vec4(0, 0, 0, 1);

redo_cross_section:
    cross_vertices.clear();
    cross_colors.clear();
    cross_tris.clear();

    tet_mesh_vertices_world.clear();
    Mat5 model = mk_model_mat(s.mesh_pos, s.mesh_scale, s.mesh_rotation);
    for (const auto& v : tet_mesh_vertices) {
        tet_mesh_vertices_world.push_back(transform(model, v));
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
        hmm_vec3 intersect[6];

        for (s32 i = 0; i < 6; i++) {
            const Edge& e = edges[i];
            hmm_vec4 l_0 = tet_mesh_vertices_world[e.v0];
            hmm_vec4 l = tet_mesh_vertices_world[e.v1] - l_0;
            if (!float_eq(HMM_Dot(l, n), 0.0, DBL_EPSILON)) {
                f64 d = HMM_Dot(p_0 - l_0, n) / HMM_Dot(l, n);
                if ((d > 0.0 && d < 1.0) || float_eq(d, 0.0, epsilon) || float_eq(d, 1.0, epsilon)) {
                    // Edge intersects with hyperplane at a point
                    hmm_vec4 point = d * l + l_0;
                    DCHECK_F(float_eq(point.W, 0.0));
                    intersect[intersect_len] = vec3(point);
                    intersect_len++;
                }
            }
        }

        s32 merged_intersect_len = 0;
        hmm_vec3 merged_intersect[6];

        {
            f64 merge_epsilon = epsilon;
            for (s32 i = 0; i < intersect_len; i++) {
                const auto& v = intersect[i];
                bool unique = true;
                for (s32 j = 0; j < intersect_len; j++) {
                    if (i != j) {
                        const auto& u = intersect[j];
                        if (float_eq(v.X, u.X, merge_epsilon) && float_eq(v.Y, u.Y, merge_epsilon)
                            && float_eq(v.Z, u.Z, merge_epsilon)) {

                            unique = false;
                            break;
                        }
                    }
                }

                if (unique) {
                    if (merged_intersect_len >= 4) {
                        // Redo merging with a greater epsilon
                        merge_epsilon *= 10.0;
                        merged_intersect_len = 0;
                        i = -1;
                        continue;
                    } else {
                        merged_intersect[merged_intersect_len] = v;
                        merged_intersect_len++;
                    }
                }
            }
        }

        CHECK_LE_F(merged_intersect_len, 4);

        if (merged_intersect_len == 3) {
            // Intersection is a triangle

            for (s32 i = 0; i < 3; i++) {
                DCHECK_EQ_F((s64)cross_vertices.size() % 3, 0);
                cross_tris.push_back((u32)(cross_vertices.size() / 3));
                for (f64 e : merged_intersect[i].Elements) {
                    cross_vertices.push_back((f32)e);
                }
                for (f32 e : tet.color) {
                    cross_colors.push_back(e);
                }
            }

        } else if (merged_intersect_len == 4) {
            // Intersection is a quadrilateral

            hmm_vec3 p0 = merged_intersect[0];
            hmm_vec3 p1 = merged_intersect[1];
            hmm_vec3 p2 = merged_intersect[2];
            hmm_vec3 p3 = merged_intersect[3];
            u32 v_mapping[4];

            for (s32 i = 0; i < 4; i++) {
                DCHECK_EQ_F((s64)cross_vertices.size() % 3, 0);
                v_mapping[i] = (u32)(cross_vertices.size() / 3);
                for (f64 e : merged_intersect[i].Elements) {
                    cross_vertices.push_back((f32)e);
                }
                for (f32 e : tet.color) {
                    cross_colors.push_back(e);
                }
            }

            hmm_vec3 l0 = p1 - p0;
            hmm_vec3 l1 = p2 - p0;
            hmm_vec3 l2 = p3 - p0;
            DCHECK_F(float_eq(HMM_Dot(HMM_Cross(l0, l1), l2), 0.0));

            f64 sum0 = HMM_LengthSquared(p1 - p0) + HMM_LengthSquared(p3 - p2);
            f64 sum1 = HMM_LengthSquared(p2 - p0) + HMM_LengthSquared(p3 - p1);
            f64 sum2 = HMM_LengthSquared(p3 - p0) + HMM_LengthSquared(p2 - p1);
            u32 tris[6];
            if (sum0 > sum1 && sum0 > sum2) {
                // p0 p1 is a diagonal
                tris[0] = 0;
                tris[1] = 1;
                tris[2] = 2;

                tris[3] = 0;
                tris[4] = 1;
                tris[5] = 3;
            } else if (sum1 > sum0 && sum1 > sum2) {
                // p0 p2 is a diagonal
                tris[0] = 0;
                tris[1] = 2;
                tris[2] = 1;

                tris[3] = 0;
                tris[4] = 2;
                tris[5] = 3;
            } else {
                // p0 p3 is a diagonal
                tris[0] = 0;
                tris[1] = 3;
                tris[2] = 1;

                tris[3] = 0;
                tris[4] = 3;
                tris[5] = 2;
            }

            for (u32 e : tris) {
                cross_tris.push_back(v_mapping[e]);
            }

        } else {
            // Less than 3 intersections
            for (s32 i = 0; i < 6; i++) {
                const Edge& e = edges[i];
                hmm_vec4 l_0 = tet_mesh_vertices_world[e.v0];
                hmm_vec4 l = tet_mesh_vertices_world[e.v1] - l_0;
                if (float_eq(HMM_Dot(l, n), 0.0, epsilon) && float_eq(HMM_Dot(p_0 - l_0, n), 0.0, epsilon)) {
                    // Edge is within hyperplane

                    // Because of floating point error, we don't try to render
                    // this case. Instead, we bump the mesh's w position and
                    // hope for points of intersection instead.
                    s.bump_mesh_pos_w();
                    goto redo_cross_section;
                }
            }
        }
    }
}

void Renderer::render() {
    auto& s = *state;

    if (s.mesh_changed) {
        s.mesh_changed = false;
        do_mesh_changed();
    }

    if (s.window_size_changed) {
        s.window_size_changed = false;
        do_window_size_changed();
    }

    hmm_mat4 view = HMM_LookAt(s.camera_pos, s.camera_target, s.camera_up);
    const f64 fov = 85.0;

    glEnable(GL_DEPTH_TEST);

    combined_buffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, combined_buffer.width, combined_buffer.height);

    hmm_mat4 vp = HMM_Perspective(fov, combined_buffer.width / (f64)combined_buffer.height, 0.01, 1000.0) * view;
    f32 vp_f32[16];
    mat4_to_f32(vp, vp_f32);
    view_projection_ubo.buffer_data(vp_f32, sizeof(vp_f32));

    // Draw cross-section
    {
        glLineWidth(0.5f);
        xz_grid.draw();

        calculate_cross_section();
        CHECK_EQ_F(cross_vertices.size(), cross_colors.size());
        cross_section.get_vbo(0).buffer_data(cross_vertices.data(), cross_vertices.size() * sizeof(f32));
        cross_section.get_vbo(1).buffer_data(cross_colors.data(), cross_colors.size() * sizeof(f32));
        cross_section.ebo.buffer_elements(cross_tris.data(), (s32)cross_tris.size());

        cross_section.draw();
    }

    // Draw projection
    {
        if (s.split) {
            projection_buffer.bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport(0, 0, projection_buffer.width, projection_buffer.height);

            hmm_mat4 vp =
                    HMM_Perspective(fov, projection_buffer.width / (f64)projection_buffer.height, 0.01, 1000.0) * view;
            f32 vp_f32[16];
            mat4_to_f32(vp, vp_f32);
            view_projection_ubo.buffer_data(vp_f32, sizeof(vp_f32));
        }

        glLineWidth(0.5f);
        xz_grid.draw();

        // Perform 4D to 3D projection

        projected_vertices.clear();
        projected_vertices3.clear();

        Mat5 model = mk_model_mat(s.mesh_pos, s.mesh_scale, s.mesh_rotation);
        Mat5 mv = mk_model_view_mat(model, s.camera4);
        for (const hmm_vec4& v : s.mesh.vertices) {
            Vec5 view_v = mv * vec5(v, 1);
            hmm_vec4 v_ = project_perspective(view_v, s.camera4.near);
            projected_vertices.push_back(v_);
            projected_vertices3.push_back(vec3(v_));
        }

        DCHECK_EQ_F(s.mesh.vertices.size(), projected_vertices.size());

        // Triangulate selected cell

        selected_cell_tri_faces.clear();
        for (u32 face_i : s.mesh.cells[(size_t)s.selected_cell]) {
            const auto& face = s.mesh.faces[face_i];
            render_funcs.triangulate(projected_vertices3, s.mesh.edges, face, selected_cell_tri_faces);
        }

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

        wireframe.get_vbo(0).buffer_data(projected_vertices_f32.data(), projected_vertices_f32.size() * sizeof(f32));
        selected_cell.ebo.buffer_elements(selected_cell_tri_faces.data(), (s32)selected_cell_tri_faces.size());

        n4d_shader_prog.set_uniform_f32("max_depth", max_depth);

        f32 selected_cell_color[3] = {1, 0, 1};
        n4d_shader_prog.set_uniform_vec3("color1", selected_cell_color);
        selected_cell.draw();

        f32 wireframe_color[3] = {1, 1, 0};
        n4d_shader_prog.set_uniform_vec3("color1", wireframe_color);
        glLineWidth(2.0f);
        wireframe.draw();
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, combined_buffer.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (s.split) {
        f64 cross_width = std::round(vis_width_screen * s.divider);
        f64 projection_width = vis_width_screen - cross_width;
        glBlitFramebuffer((s32)(combined_buffer.width / 2.0 - cross_width / 2.0), 0,
                          (s32)(combined_buffer.width / 2.0 + cross_width / 2.0), s.window_height, 0, 0,
                          (s32)cross_width, s.window_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, projection_buffer.id);
        glBlitFramebuffer((s32)(projection_buffer.width / 2.0 - projection_width / 2.0), 0,
                          (s32)(projection_buffer.width / 2.0 + projection_width / 2.0), s.window_height,
                          (s32)cross_width, 0, (s32)(cross_width + projection_width), s.window_height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Draw divider bar

        bind_default_framebuffer();
        glDisable(GL_DEPTH_TEST);
        glViewport(0, 0, s.window_width, s.window_height);

        f64 divider_x_pos = (s.visualization_width * s.divider) * 2.0 - 1.0 - divider_width / 2.0;
        divider_bar_shader_prog.set_uniform_f32("x_pos", (f32)divider_x_pos);
        divider_bar.draw();

    } else {
        glBlitFramebuffer(0, 0, combined_buffer.width, s.window_height, 0, 0, combined_buffer.width, s.window_height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    bind_default_framebuffer();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

#ifdef FOUR_DEBUG
    switch (glGetError()) {
    case GL_NO_ERROR:
        break;

    case GL_INVALID_ENUM:
        ABORT_F("GL_INVALID_ENUM");

    case GL_INVALID_VALUE:
        ABORT_F("GL_INVALID_VALUE");

    case GL_INVALID_OPERATION:
        ABORT_F("GL_INVALID_OPERATION");

    case GL_INVALID_FRAMEBUFFER_OPERATION:
        ABORT_F("GL_INVALID_FRAMEBUFFER_OPERATION");

    case GL_OUT_OF_MEMORY:
        ABORT_F("GL_OUT_OF_MEMORY");

    default:
        ABORT_F("Unknown OpenGL error");
    }
#endif
}
} // namespace four
