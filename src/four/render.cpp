#include <four/math.hpp>
#include <four/render.hpp>

#include <igl/triangle/triangulate.h>
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

ShaderProgram::ShaderProgram(const char* vert_path, const char* frag_path) {
    u32 vert_shader = compile_shader(vert_path, GL_VERTEX_SHADER);
    u32 frag_shader = compile_shader(frag_path, GL_FRAGMENT_SHADER);

    id = glCreateProgram();
    glAttachShader(id, vert_shader);
    glAttachShader(id, frag_shader);
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

void ShaderProgram::set_uniform_mat4(const char* name, const f32* data) {
    auto it = uniform_locations.find(name);
    s32 location;
    if (it == uniform_locations.end()) {
        location = glGetUniformLocation(id, name);
        CHECK_NE_F(location, -1);
        uniform_locations.emplace(name, location);
    } else {
        location = it->second;
    }

    glUseProgram(id);
    glUniformMatrix4fv(location, 1, false, data);
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

void Renderer::update_mesh_buffers() {
    auto& s = *state;

    mesh_colors.clear();
    for (size_t i = 0; i < s.mesh.vertices.size(); i++) {
        f32 color[3] = {1, 0, 0};
        for (f32 e : color) {
            mesh_colors.push_back(e);
        }
    }
    wireframe.vbos[1]->buffer_data_realloc(mesh_colors.data(), mesh_colors.size() * sizeof(f32));

    mesh_colors.clear();
    for (size_t i = 0; i < s.mesh.vertices.size(); i++) {
        f32 color[3] = {1, 1, 0};
        for (f32 e : color) {
            mesh_colors.push_back(e);
        }
    }
    selected_cell.vbos[1]->buffer_data_realloc(mesh_colors.data(), mesh_colors.size() * sizeof(f32));

    wireframe.ebo.buffer_data_realloc(s.mesh.edges.data(), 2 * (s32)s.mesh.edges.size());
}

Renderer::Renderer(SDL_Window* window, AppState* state) : window(window), state(state) {
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

    shader_program = ShaderProgram("data/vertex.glsl", "data/fragment.glsl");

    // Wireframe
    // ---------

    size_t wireframe_vertices = add_vbo(GL_STREAM_DRAW);
    VertexSpec wireframe_vertices_spec = {
            .index = 0,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    size_t wireframe_colors = add_vbo(GL_STATIC_DRAW);
    VertexSpec wireframe_colors_spec = {
            .index = 1,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    // ---------

    // Selected cell
    // -------------

    size_t selected_cell_colors = add_vbo(GL_STATIC_DRAW);

    // -------------

    // XZ grid
    // -------

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

    size_t xz_grid_colors = add_vbo(GL_STATIC_DRAW);
    std::vector<f32> xz_grid_colors_vec;
    for (size_t i = 0; i < xz_grid_vertices_vec.size(); i++) {
        f32 color[3] = {0.5f, 0.5f, 0.5f};
        for (f32 e : color) {
            xz_grid_colors_vec.push_back(e);
        }
    }
    vbos[xz_grid_colors].buffer_data(xz_grid_colors_vec.data(), xz_grid_colors_vec.size() * sizeof(f32));

    // -------

    // Wireframe VAO
    // -------------

    ElementBufferObject wireframe_ebo(GL_STATIC_DRAW, GL_LINES);

    VertexBufferObject* wireframe_vbos[] = {&vbos[wireframe_vertices], &vbos[wireframe_colors]};
    VertexSpec wireframe_vertex_specs[] = {wireframe_vertices_spec, wireframe_colors_spec};
    wireframe = VertexArrayObject(&shader_program, AS_SLICE(wireframe_vbos), AS_SLICE(wireframe_vertex_specs),
                                  wireframe_ebo);
    // -------------

    // Selected cell VAO
    // -----------------

    ElementBufferObject cell_ebo(GL_STREAM_DRAW, GL_TRIANGLES);

    VertexBufferObject* selected_cell_vbos[] = {&vbos[wireframe_vertices], &vbos[selected_cell_colors]};
    selected_cell = VertexArrayObject(&shader_program, AS_SLICE(selected_cell_vbos), AS_SLICE(wireframe_vertex_specs),
                                      cell_ebo);

    // -----------------

    // XZ grid VAO
    // -----------

    ElementBufferObject xz_grid_ebo(GL_STATIC_DRAW, GL_LINES);
    std::vector<u32> xz_grid_indices_vec;
    for (u32 i = 0; i < xz_grid_vertices_vec.size(); i++) {
        xz_grid_indices_vec.push_back(i);
    }
    xz_grid_ebo.buffer_data(xz_grid_indices_vec.data(), (s32)xz_grid_indices_vec.size());

    VertexBufferObject* xz_grid_vbos[] = {&vbos[xz_grid_vertices], &vbos[xz_grid_colors]};
    xz_grid = VertexArrayObject(&shader_program, AS_SLICE(xz_grid_vbos), AS_SLICE(wireframe_vertex_specs), xz_grid_ebo);

    // -----------

    update_mesh_buffers();
}

void Renderer::render() {
    auto& s = *state;

    if (s.window_size_changed) {
        s.window_size_changed = false;
        update_window_size();
    }

    if (s.mesh_changed) {
        s.mesh_changed = false;
        update_mesh_buffers();
    }

    // Perform 4D to 3D projection
    {
        projected_vertices.clear();

        Mat5 mv = mk_model_view_mat(s.mesh_pos, s.mesh_scale, s.mesh_rotation, s.camera4);
        for (const hmm_vec4& v : s.mesh.vertices) {
            Vec5 view_v = mv * vec5(v, 1);
            hmm_vec3 n3d_v = vec3(project_perspective(view_v, s.camera4.near));
            projected_vertices.push_back(n3d_v);
        }
        DCHECK_EQ_F(s.mesh.vertices.size(), projected_vertices.size());
    }

    // Triangulate selected cell
    selected_cell_tri_faces.clear();
    for (u32 face_i : s.mesh.cells[(size_t)s.selected_cell]) {
        const auto& face = s.mesh.faces[face_i];
        triangulate(projected_vertices, s.mesh.edges, face, selected_cell_tri_faces);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    projected_vertices_f32.clear();
    for (const auto& v : projected_vertices) {
        for (f64 element : v.Elements) {
            projected_vertices_f32.push_back((f32)element);
        }
    }

    wireframe.vbos[0]->buffer_data(projected_vertices_f32.data(), projected_vertices_f32.size() * 3 * sizeof(f32));
    selected_cell.ebo.buffer_data(selected_cell_tri_faces.data(), (s32)selected_cell_tri_faces.size());

    hmm_mat4 view = HMM_LookAt(s.camera_pos, s.camera_target, s.camera_up);
    hmm_mat4 vp = projection * view;

    f32 vp_f32[16];
    for (s32 col = 0; col < 4; col++) {
        for (s32 row = 0; row < 4; row++) {
            vp_f32[col * 4 + row] = (f32)vp[col][row];
        }
    }

    shader_program.set_uniform_mat4("vp", vp_f32);

    glLineWidth(0.5f);
    xz_grid.draw();

    selected_cell.draw();

    glLineWidth(2.0f);
    wireframe.draw();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}

void Renderer::triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                           std::vector<u32>& out) {

    using VertexIMapping = TriangulateState::VertexIMapping;
    auto& s = triangulate_state;
    const f64 epsilon = 0.00000000000001;

    // Calculate normal vector

    const auto& edge0 = edges[face[0]];
    hmm_vec3 v0 = vertices[edge0.v0];
    hmm_vec3 l0 = v0 - vertices[edge0.v1];

    hmm_vec3 normal;
    for (size_t i = 0; i < face.size(); i++) {
        const auto& edge = edges[face[i]];
        hmm_vec3 l1 = vertices[edge.v0] - vertices[edge.v1];
        normal = HMM_Cross(l0, l1);
        if (float_eq(normal.X, 0.0) && float_eq(normal.Y, 0.0) && float_eq(normal.Z, 0.0)) {
            // `normal` is the zero vector

            // Fail if there are no more edges---this means the face has
            // no surface area
            CHECK_LT_F(i, face.size() - 1);
        } else {
            break;
        }
    }

    normal = HMM_Normalize(normal);

    // Calculate transformation to 2D

    hmm_vec3 up;
    if (float_eq(std::abs(normal.Y), 1.0, 0.001)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    hmm_mat4 to_2d_trans = HMM_LookAt(v0, v0 + normal, up);

    s.face2_vertex_i_mapping.clear();
    s.face2_vertices.clear();
    s.face2_edges.clear();

    for (u32 edge_i : face) {
        const auto& e = edges[edge_i];
        for (u32 v_i : e.vertices) {
            if (s.face2_vertex_i_mapping.left.find(v_i) == s.face2_vertex_i_mapping.left.end()) {
                s.face2_vertex_i_mapping.left.insert(
                        VertexIMapping::left_value_type(v_i, (u32)s.face2_vertices.size()));
                hmm_vec3 v_ = transform(to_2d_trans, vertices[v_i]);
                DCHECK_F(float_eq(v_.Z, 0.0, epsilon));
                s.face2_vertices.push_back(vec2(v_));
            }
        }
        s.face2_edges.push_back(edge(s.face2_vertex_i_mapping.left.at(e.v0), s.face2_vertex_i_mapping.left.at(e.v1)));
    }

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : s.face2_vertex_i_mapping.left) {
        if (entry.first != edge0.v0) {
            hmm_vec3 v = vertices[entry.first];
            f64 x = HMM_Dot(v - v0, normal);

            // NOTE: Consider floating-point error. See
            // https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
            DCHECK_F(float_eq(x, 0.0, epsilon));
        }
    }
#endif

    // `igl::triangle::triangulate()` only accepts double precision
    // floating point values.
    s.mesh_v.resize((s64)s.face2_vertices.size(), Eigen::NoChange);
    for (size_t i = 0; i < s.face2_vertices.size(); i++) {
        hmm_vec2 v = s.face2_vertices[i];
        s.mesh_v((s64)i, 0) = v.X;
        s.mesh_v((s64)i, 1) = v.Y;
    }

    s.mesh_e.resize((s64)s.face2_edges.size(), Eigen::NoChange);
    for (size_t i = 0; i < s.face2_edges.size(); i++) {
        Edge e = s.face2_edges[i];
        s.mesh_e((s64)i, 0) = (s32)e.v0;
        s.mesh_e((s64)i, 1) = (s32)e.v1;
    }

    // TODO: Support triangulating faces with holes
    const Eigen::MatrixX2d h;

    s.triangulate_out_v.resize(0, Eigen::NoChange);
    s.triangulate_out_f.resize(0, Eigen::NoChange);
    igl::triangle::triangulate(s.mesh_v, s.mesh_e, h, "Q", s.triangulate_out_v, s.triangulate_out_f);

    DCHECK_EQ_F((size_t)s.triangulate_out_v.rows(), s.face2_vertices.size());

#ifdef FOUR_DEBUG
    for (s32 i = 0; i < s.triangulate_out_v.rows(); i++) {
        DCHECK_F(float_eq(s.triangulate_out_v(i, 0), s.face2_vertices[(size_t)i].X));
        DCHECK_F(float_eq(s.triangulate_out_v(i, 1), s.face2_vertices[(size_t)i].Y));
    }
#endif

    for (s32 i = 0; i < s.triangulate_out_f.rows(); i++) {
        for (s32 j = 0; j < 3; j++) {
            out.push_back(s.face2_vertex_i_mapping.right.at(s.triangulate_out_f(i, j)));
        }
    }
}
} // namespace four
