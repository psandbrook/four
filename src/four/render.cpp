#include <four/render.hpp>

#include <four/resource.hpp>

#include <SDL.h>
#include <earcut.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace four {

namespace {

constexpr f64 divider_width = 0.007;

struct ConstFaceRef final : public std::reference_wrapper<const std::vector<glm::dvec2>> {
    using value_type = type::value_type;

    explicit ConstFaceRef(const type& a) noexcept : std::reference_wrapper<type>(a) {}

    type::size_type size() const noexcept {
        return get().size();
    }

    bool empty() const noexcept {
        return get().empty();
    }

    type::const_reference operator[](type::size_type pos) const {
        return get()[pos];
    }
};

void mat4_to_f32(const glm::dmat4& mat, f32* out) {
    for (s32 col = 0; col < 4; col++) {
        for (s32 row = 0; row < 4; row++) {
            out[col * 4 + row] = (f32)mat[col][row];
        }
    }
}

u32 compile_shader(const char* relative_shader_path, GLenum type) {
    auto relative_path = std::string("shaders/") + relative_shader_path;
    std::string path = get_resource_path(relative_path.c_str());

    std::ifstream stream(path);
    CHECK_F(bool(stream), "Could not open file \"%s\"", path.c_str());

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

GlBuffer::GlBuffer(GLenum type, GLenum usage) : type(type), usage(usage) {
    glGenBuffers(1, &id);
    bind();
}

void GlBuffer::bind() {
    glBindBuffer(type, id);
}

void GlBuffer::buffer_data(const void* data, size_t size) {
    bind();
    if (this->size < size) {
        glBufferData(type, size, data, usage);
        this->size = size;
    } else {
        CHECK_NE_F(usage, (u32)GL_STATIC_DRAW);
        glBufferSubData(type, 0, size, data);
    }
}

void GlBuffer::buffer_data_realloc(const void* data, size_t size) {
    bind();
    glBufferData(type, size, data, usage);
    this->size = size;
}

void GlBuffer::destroy() {
    glDeleteBuffers(1, &id);
    *this = GlBuffer();
}

ShaderProgram::ShaderProgram(u32 vertex_shader, std::initializer_list<u32> fragment_shaders) {
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);

    for (u32 e : fragment_shaders) {
        glAttachShader(id, e);
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

    constexpr u32 samples = 8;

    glGenRenderbuffers(2, rbos);
    glBindRenderbuffer(GL_RENDERBUFFER, color_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_SRGB8, width, height);
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
    glDeleteRenderbuffers(2, rbos);
    *this = Framebuffer();
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

VertexArrayObject::VertexArrayObject(ShaderProgram* shader_program,
                                     std::unordered_map<u32, VertexBufferObject>* vbos_ptr,
                                     std::initializer_list<u32> vbos_, std::initializer_list<VertexSpec> specs,
                                     ElementBufferObject ebo)
        : shader_program(shader_program), vbos_ptr(vbos_ptr), vbos(vbos_.begin(), vbos_.end()), ebo(ebo) {

    for (u32 i = 0; i < vbos.size(); i++) {
        auto& vbo = get_vbo(i);
        CHECK_EQ_F(vbo.type, (u32)GL_ARRAY_BUFFER);
    }

    CHECK_EQ_F(ebo.type, (u32)GL_ELEMENT_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    for (u32 i = 0; i < vbos.size(); i++) {
        auto& vbo = get_vbo(i);
        VertexSpec spec = specs.begin()[i];
        vbo.bind();
        glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride, (void*)spec.offset);
        glEnableVertexAttribArray(spec.index);
    }

    ebo.bind();

    glBindVertexArray(0);
}

VertexBufferObject& VertexArrayObject::get_vbo(u32 id) {
    return vbos_ptr->at(vbos.at(id));
}

void VertexArrayObject::draw() {
    glUseProgram(shader_program->id);
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.primitive_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void VertexArrayObject::destroy() {
    glDeleteVertexArrays(1, &id);
    ebo.destroy();
    *this = VertexArrayObject();
}

Renderer::Renderer(AppState* state)
        : state(state), color_dist(0.0f, std::nextafter(1.0f, std::numeric_limits<f32>::max())) {

    SDL_GL_SetSwapInterval(1);

    constexpr f32 bg_shade = 0.04f;
    glClearColor(bg_shade, bg_shade, bg_shade, 1.0f);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0, -1.0);

    do_window_size_changed();

    view_projection_ubo = UniformBufferObject("ViewProjection", 0, GL_STREAM_DRAW);

    // Wireframe & selected cell
    {
        u32 vert_shader = compile_shader("n4d.vert", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("n4d.frag", GL_FRAGMENT_SHADER);
        n4d_shader_prog = ShaderProgram(vert_shader, {frag_shader});
    }

    // Cross-section
    {
        u32 vert_shader = compile_shader("cross.vert", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("cross.frag", GL_FRAGMENT_SHADER);
        cross_section_shader_prog = ShaderProgram(vert_shader, {frag_shader});
    }

    // XZ grid
    {
        u32 vert_shader = compile_shader("xz-grid.vert", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("xz-grid.frag", GL_FRAGMENT_SHADER);
        xz_grid_shader_prog = ShaderProgram(vert_shader, {frag_shader});

        u32 xz_grid_vertices_vbo = add_vbo(GL_STATIC_DRAW);
        std::vector<f32> grid_vertices;
        constexpr s32 n_grid_lines = 20;
        constexpr f64 grid_lines_spacing = 0.2;

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

        VertexSpec vertex_spec = {};
        vertex_spec.index = 0;
        vertex_spec.size = 3;
        vertex_spec.type = GL_FLOAT;
        vertex_spec.stride = 3 * sizeof(f32);
        vertex_spec.offset = 0;

        vbos.at(xz_grid_vertices_vbo).buffer_data(grid_vertices.data(), grid_vertices.size() * sizeof(f32));

        ElementBufferObject ebo(GL_STATIC_DRAW, GL_LINES);
        std::vector<u32> indices_vec;
        for (u32 i = 0; i < grid_vertices.size(); i++) {
            indices_vec.push_back(i);
        }
        ebo.buffer_elements(indices_vec.data(), (s32)indices_vec.size());

        xz_grid_vao = VertexArrayObject(&xz_grid_shader_prog, &vbos, {xz_grid_vertices_vbo}, {vertex_spec}, ebo);
    }

    ShaderProgram* shader_progs[] = {&n4d_shader_prog, &cross_section_shader_prog, &xz_grid_shader_prog};
    for (auto prog : shader_progs) {
        prog->bind_uniform_block(view_projection_ubo);
    }

    // Divider bar
    {
        u32 vert_shader = compile_shader("divider.vert", GL_VERTEX_SHADER);
        u32 frag_shader = compile_shader("divider.frag", GL_FRAGMENT_SHADER);
        divider_bar_shader_prog = ShaderProgram(vert_shader, {frag_shader});

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

        VertexSpec vertex_spec = {};
        vertex_spec.index = 0;
        vertex_spec.size = 3;
        vertex_spec.type = GL_FLOAT;
        vertex_spec.stride = 3 * sizeof(f32);
        vertex_spec.offset = 0;

        vbos.at(vertices_vbo).buffer_data(vertices, sizeof(vertices));

        ElementBufferObject ebo(GL_STATIC_DRAW, GL_TRIANGLES);
        ebo.buffer_elements(indices, ARRAY_SIZE(indices));
        divider_bar_vao = VertexArrayObject(&divider_bar_shader_prog, &vbos, {vertices_vbo}, {vertex_spec}, ebo);
    }

    do_mesh_instances_changed();
}

u32 Renderer::add_vbo(GLenum usage) {
    while (has_key(vbos, next_vbo_id)) {
        ++next_vbo_id;
    }

    const u32 id = next_vbo_id;
    ++next_vbo_id;
    vbos.emplace(id, VertexBufferObject(usage));
    return id;
}

void Renderer::destroy_vbo(u32 id) {
    vbos.at(id).destroy();
    vbos.erase(id);
}

void Renderer::do_window_size_changed() {
    combined_buffer.destroy();
    projection_buffer.destroy();

    vis_width_screen = (u32)(state->window_width - state->ui_size_screen);
    combined_buffer = Framebuffer(vis_width_screen, (u32)state->window_height);
    projection_buffer = Framebuffer(vis_width_screen, (u32)state->window_height);
}

void Renderer::do_mesh_instances_changed() {

    for (const auto& event : state->mesh_instances_events) {
        switch (event.type) {
        case AppState::MeshInstancesEvent::Type::added: {

            auto& mesh = state->meshes.at(state->mesh_instances.at(event.index));

            // Wireframe & selected cell
            {
                u32 wireframe_vertices = add_vbo(GL_STREAM_DRAW);
                VertexSpec vertex_spec = {};
                vertex_spec.index = 0;
                vertex_spec.size = 4;
                vertex_spec.type = GL_FLOAT;
                vertex_spec.stride = 4 * sizeof(f32);
                vertex_spec.offset = 0;

                ElementBufferObject wireframe_ebo(GL_STATIC_DRAW, GL_LINES);

                wireframe_vaos.push_back(
                        VertexArrayObject(&n4d_shader_prog, &vbos, {wireframe_vertices}, {vertex_spec}, wireframe_ebo));

                ElementBufferObject selected_cell_ebo(GL_STREAM_DRAW, GL_TRIANGLES);
                selected_cell_vaos.push_back(VertexArrayObject(&n4d_shader_prog, &vbos, {wireframe_vertices},
                                                               {vertex_spec}, selected_cell_ebo));

                auto& wireframe_vao = wireframe_vaos.back();
                wireframe_vao.ebo.buffer_elements_realloc(mesh.edges.data(), 2 * (s32)mesh.edges.size());
            }

            // Cross-section
            {
                u32 vertices = add_vbo(GL_STREAM_DRAW);
                u32 colors = add_vbo(GL_STREAM_DRAW);

                VertexSpec vertex_spec = {};
                vertex_spec.index = 0;
                vertex_spec.size = 3;
                vertex_spec.type = GL_FLOAT;
                vertex_spec.stride = 3 * sizeof(f32);
                vertex_spec.offset = 0;

                VertexSpec color_spec = {};
                color_spec.index = 1;
                color_spec.size = 3;
                color_spec.type = GL_FLOAT;
                color_spec.stride = 3 * sizeof(f32);
                color_spec.offset = 0;

                ElementBufferObject ebo(GL_STREAM_DRAW, GL_TRIANGLES);
                cross_section_vaos.push_back(VertexArrayObject(&cross_section_shader_prog, &vbos, {vertices, colors},
                                                               {vertex_spec, color_spec}, ebo));

                std::vector<glm::vec3> mesh_instance_tet_colors;
                cell_colors.clear();

                for (const Mesh4::Tet& tet : mesh.tets) {
                    if (!has_key(cell_colors, tet.cell)) {
                        cell_colors.emplace(tet.cell, random_color());
                    }
                    mesh_instance_tet_colors.push_back(cell_colors.at(tet.cell));
                }

                CHECK_EQ_F(cell_colors.size(), mesh.cells.size());
                tet_colors.push_back(std::move(mesh_instance_tet_colors));
            }
        } break;

        case AppState::MeshInstancesEvent::Type::removed: {

            auto& wireframe_vao = wireframe_vaos.at(event.index);
            for (u32 i = 0; i < wireframe_vao.vbos.size(); i++) {
                destroy_vbo(wireframe_vao.vbos.at(i));
            }

            wireframe_vao.destroy();
            remove(wireframe_vaos, event.index);

            selected_cell_vaos.at(event.index).destroy();
            remove(selected_cell_vaos, event.index);

            auto& cross_section_vao = cross_section_vaos.at(event.index);
            for (u32 i = 0; i < cross_section_vao.vbos.size(); i++) {
                destroy_vbo(cross_section_vao.vbos.at(i));
            }

            cross_section_vao.destroy();
            remove(cross_section_vaos, event.index);
            remove(tet_colors, event.index);
        } break;
        }
    }

    state->mesh_instances_events.clear();
}

void Renderer::triangulate(const std::vector<glm::dvec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                           std::vector<u32>& out) {

    // Calculate normal vector

    u32 edge0_i = face[0];
    const Edge& edge0 = edges[edge0_i];
    u32 v0_i = edge0.v0;
    glm::dvec3 v0 = vertices[v0_i];
    glm::dvec3 edge0_vec = vertices[edge0.v1] - v0;

    glm::dvec3 normal;
    {
        bool found_normal = false;
        glm::dvec3 other_edge_vec;
        glm::dvec3 cross;

        for (u32 e_i : face) {
            if (e_i == edge0_i) {
                continue;
            }

            const Edge& e = edges[e_i];
            if (e.v0 == v0_i || e.v1 == v0_i) {
                u32 other_vi = e.v0 == v0_i ? e.v1 : e.v0;
                other_edge_vec = vertices[other_vi] - v0;
                cross = glm::cross(edge0_vec, other_edge_vec);
                if (!float_eq(cross, glm::dvec3(0.0))) {
                    found_normal = true;
                    break;
                }
            }
        }

        if (!found_normal) {
            // Don't triangulate if the face is degenerate.
            return;
        }

        normal = glm::normalize(cross);
    }

    // Calculate transformation to 2D

    glm::dvec3 up;
    if (float_eq(std::abs(normal.y), 1.0, 0.001)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    glm::dmat4 to_2d_trans = glm::lookAt(v0, v0 + normal, up);

    face2_vertex_i_mapping_left.clear();
    face2_vertex_i_mapping_right.clear();
    face2_vertices.clear();

    const auto add_face2_vertex = [&](u32 v_i) -> bool {
        glm::dvec3 v = vertices[v_i];
        for (const auto& entry : face2_vertex_i_mapping_left) {
            glm::dvec3 u = vertices[entry.first];
            if (float_eq(v, u)) {
                // Don't triangulate if there are duplicate vertices.
                return false;
            }
        }

        face2_vertex_i_mapping_left.emplace(v_i, (u32)face2_vertices.size());
        face2_vertex_i_mapping_right.emplace((u32)face2_vertices.size(), v_i);

        glm::dvec3 v_ = transform(to_2d_trans, v);
        DCHECK_F(float_eq(v_.z, 0.0));
        face2_vertices.push_back(glm::dvec2(v_));
        return true;
    };

    if (!add_face2_vertex(v0_i)) {
        return;
    }

    u32 prev_edge_i = edge0_i;
    u32 current_v_i = edge0.v1;

    if (!add_face2_vertex(current_v_i)) {
        return;
    }

    while (true) {
        for (u32 e_i : face) {
            if (e_i != prev_edge_i) {
                const Edge& e = edges[e_i];
                if (e.v0 == current_v_i || e.v1 == current_v_i) {
                    u32 v_i = e.v0 == current_v_i ? e.v1 : e.v0;
                    if (v_i == v0_i) {
                        goto end_face2_loop;
                    }

                    if (!add_face2_vertex(v_i)) {
                        return;
                    }
                    prev_edge_i = e_i;
                    current_v_i = v_i;
                    goto find_next_v_i;
                }
            }
        }
        ABORT_F("Invalid face");
    find_next_v_i:;
    }
end_face2_loop:

    DCHECK_EQ_F(face2_vertex_i_mapping_left.size(), face2_vertex_i_mapping_right.size());

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : face2_vertex_i_mapping_left) {
        if (entry.first != edge0.v0) {
            glm::dvec3 v = vertices[entry.first];
            f64 x = glm::dot(v - v0, normal);
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    const std::array<ConstFaceRef, 1> polygon = {ConstFaceRef(face2_vertices)};
    const std::vector<u32> result_indices = mapbox::earcut(polygon);

    for (u32 v_i : result_indices) {
        out.push_back(face2_vertex_i_mapping_right.at(v_i));
    }
}

void Renderer::calculate_cross_section(const u32 mesh_instance, std::vector<f32>& out_vertices,
                                       std::vector<f32>& out_colors, std::vector<u32>& out_tris) {

    const glm::dvec4 p_0 = glm::dvec4(0, 0, 0, 0);
    const glm::dvec4 n = glm::dvec4(0, 0, 0, 1);

redo_cross_section:
    auto& mesh = state->meshes.at(state->mesh_instances.at(mesh_instance));
    auto& mesh_transform = state->mesh_instances_transforms.at(mesh_instance);
    auto& mesh_tet_colors = tet_colors.at(mesh_instance);

    out_vertices.clear();
    out_colors.clear();
    out_tris.clear();

    tet_mesh_vertices_world.clear();
    Mat5 model = mk_model_mat(mesh_transform);
    for (const auto& v : mesh.tet_vertices) {
        tet_mesh_vertices_world.push_back(transform(model, v));
    }

    for (size_t tet_i = 0; tet_i < mesh.tets.size(); tet_i++) {
        const Mesh4::Tet& tet = mesh.tets.at(tet_i);
        const glm::vec3& color = mesh_tet_colors.at(tet_i);

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

        BoundedVector<glm::dvec3, 6> intersect;

        for (s32 i = 0; i < 6; i++) {
            const Edge& e = edges[i];
            glm::dvec4 l_0 = tet_mesh_vertices_world[e.v0];
            glm::dvec4 l = tet_mesh_vertices_world[e.v1] - l_0;

            if (!float_eq(glm::dot(l, n), 0.0)) {
                f64 d = glm::dot(p_0 - l_0, n) / glm::dot(l, n);

                if ((d >= 0.0 && d <= 1.0) || float_eq(d, 0.0) || float_eq(d, 1.0)) {
                    // Edge intersects with hyperplane at a point
                    glm::dvec4 point = d * l + l_0;
                    DCHECK_F(float_eq(point.w, 0.0));

                    glm::dvec3 point3 = glm::dvec3(point);
                    bool unique = true;
                    for (const auto& v : intersect) {
                        if (float_eq(point3, v)) {
                            unique = false;
                            break;
                        }
                    }

                    if (unique) {
                        intersect.push_back(point3);
                    }
                }

            } else if (float_eq(glm::dot(p_0 - l_0, n), 0.0)) {
                // Edge is within hyperplane

                // Because of floating point error, we don't try to render
                // this case. Instead, we bump the mesh's w position and
                // hope for points of intersection instead.
                state->bump_mesh_pos_w(mesh_instance);
                goto redo_cross_section;
            }
        }

        DCHECK_LE_F(intersect.len, 4u);

        if (intersect.len == 3) {
            // Intersection is a triangle

            for (s32 i = 0; i < 3; i++) {
                DCHECK_EQ_F((s64)out_vertices.size() % 3, 0);
                out_tris.push_back((u32)(out_vertices.size() / 3));
                for (s32 j = 0; j < 3; j++) {
                    out_vertices.push_back((f32)intersect[(size_t)i][j]);
                    out_colors.push_back(color[j]);
                }
            }

        } else if (intersect.len == 4) {
            // Intersection is a quadrilateral

            glm::dvec3 p0 = intersect[0];
            glm::dvec3 p1 = intersect[1];
            glm::dvec3 p2 = intersect[2];
            glm::dvec3 p3 = intersect[3];
            u32 v_mapping[4];

            for (s32 i = 0; i < 4; i++) {
                DCHECK_EQ_F((s64)out_vertices.size() % 3, 0);
                v_mapping[i] = (u32)(out_vertices.size() / 3);
                for (s32 j = 0; j < 3; j++) {
                    out_vertices.push_back((f32)intersect[(size_t)i][j]);
                    out_colors.push_back(color[j]);
                }
            }

            glm::dvec3 l0 = p1 - p0;
            glm::dvec3 l1 = p2 - p0;
            glm::dvec3 l2 = p3 - p0;
            DCHECK_F(float_eq(glm::dot(glm::cross(l0, l1), l2), 0.0));

            f64 sum0 = glm::length(p1 - p0) + glm::length(p3 - p2);
            f64 sum1 = glm::length(p2 - p0) + glm::length(p3 - p1);
            f64 sum2 = glm::length(p3 - p0) + glm::length(p2 - p1);
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
                out_tris.push_back(v_mapping[e]);
            }
        }
    }
}

glm::vec3 Renderer::random_color() {
    return glm::vec3(color_dist(state->random_eng_32), color_dist(state->random_eng_32),
                     color_dist(state->random_eng_32));
}

void Renderer::render() {

    if (state->window_size_changed) {
        state->window_size_changed = false;
        do_window_size_changed();
    }

    do_mesh_instances_changed();

    if (state->wireframe_render) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glm::dmat4 view = glm::lookAt(state->camera_pos, state->camera_target, state->camera_up);
    const f64 fov = glm::radians(60.0);

    glEnable(GL_DEPTH_TEST);

    bind_default_framebuffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::dmat4 combined_vp =
            glm::perspective(fov, combined_buffer.width / (f64)combined_buffer.height, 0.01, 1000.0) * view;
    f32 combined_vp_f32[16];
    mat4_to_f32(combined_vp, combined_vp_f32);

    glm::dmat4 projection_vp =
            glm::perspective(fov, projection_buffer.width / (f64)projection_buffer.height, 0.01, 1000.0) * view;
    f32 projection_vp_f32[16];
    mat4_to_f32(projection_vp, projection_vp_f32);

    const auto bind_combined_buffer = [&]() {
        combined_buffer.bind();
        glViewport(0, 0, combined_buffer.width, combined_buffer.height);
        view_projection_ubo.buffer_data(combined_vp_f32, sizeof(combined_vp_f32));
    };

    const auto bind_projection_buffer = [&]() {
        if (state->split) {
            projection_buffer.bind();
            glViewport(0, 0, projection_buffer.width, projection_buffer.height);
            view_projection_ubo.buffer_data(projection_vp_f32, sizeof(projection_vp_f32));
        }
    };

    bind_combined_buffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLineWidth(0.5f);
    xz_grid_vao.draw();

    bind_projection_buffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    xz_grid_vao.draw();

    for (u32 mesh_instance = 0; mesh_instance < state->mesh_instances.size(); mesh_instance++) {

        // Draw cross-section
        {
            bind_combined_buffer();
            calculate_cross_section(mesh_instance, cross_vertices, cross_colors, cross_tris);
            DCHECK_EQ_F(cross_vertices.size(), cross_colors.size());

            auto& cross_section_vao = cross_section_vaos.at(mesh_instance);
            cross_section_vao.get_vbo(0).buffer_data(cross_vertices.data(), cross_vertices.size() * sizeof(f32));
            cross_section_vao.get_vbo(1).buffer_data(cross_colors.data(), cross_colors.size() * sizeof(f32));
            cross_section_vao.ebo.buffer_elements(cross_tris.data(), (s32)cross_tris.size());
            cross_section_vao.draw();
        }

        // Draw projection
        {
            bind_projection_buffer();

            // Perform 4D to 3D projection
            auto& mesh = state->meshes.at(state->mesh_instances.at(mesh_instance));
            auto& mesh_transform = state->mesh_instances_transforms.at(mesh_instance);

            projected_vertices.clear();
            projected_vertices3.clear();

            Mat5 model = mk_model_mat(mesh_transform);
            Mat5 mv = mk_model_view_mat(model, state->camera4);
            for (const glm::dvec4& v : mesh.vertices) {
                Vec5 view_v = mv * Vec5(v, 1);

                glm::dvec4 v_;
                if (state->perspective_projection) {
                    v_ = project_perspective(view_v, state->camera4.near);
                } else {
                    v_ = project_orthographic(view_v, state->camera4.near);
                }

                projected_vertices.push_back(v_);
                projected_vertices3.push_back(glm::dvec3(v_));
            }

            DCHECK_EQ_F(mesh.vertices.size(), projected_vertices.size());

            f32 max_depth = 0.0f;
            projected_vertices_f32.clear();
            for (const glm::dvec4& v : projected_vertices) {
                if (v.w > max_depth) {
                    max_depth = (f32)v.w;
                }
                for (s32 i = 0; i < 4; i++) {
                    f64 element = v[i];
                    projected_vertices_f32.push_back((f32)element);
                }
            }

            auto& wireframe_vao = wireframe_vaos.at(mesh_instance);
            wireframe_vao.get_vbo(0).buffer_data(projected_vertices_f32.data(),
                                                 projected_vertices_f32.size() * sizeof(f32));
            n4d_shader_prog.set_uniform_f32("max_depth", max_depth);

            if (mesh_instance == state->selected_mesh_instance && state->selected_cell_enabled) {
                selected_cell_tri_faces.clear();
                for (u32 face_i : mesh.cells[(size_t)state->selected_cell]) {
                    const auto& face = mesh.faces[face_i];
                    triangulate(projected_vertices3, mesh.edges, face, selected_cell_tri_faces);
                }

                auto& selected_cell_vao = selected_cell_vaos.at(mesh_instance);
                selected_cell_vao.ebo.buffer_elements(selected_cell_tri_faces.data(),
                                                      (s32)selected_cell_tri_faces.size());
                f32 selected_cell_color[3] = {1, 0, 1};
                n4d_shader_prog.set_uniform_vec3("color1", selected_cell_color);
                selected_cell_vao.draw();
            }

            f32 wireframe_color[3] = {1, 1, 0};
            n4d_shader_prog.set_uniform_vec3("color1", wireframe_color);
            glLineWidth(2.0f);
            wireframe_vao.draw();
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, combined_buffer.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (state->split) {
        f64 cross_width = std::round(vis_width_screen * state->divider);
        f64 projection_width = vis_width_screen - cross_width;
        glBlitFramebuffer((s32)(combined_buffer.width / 2.0 - cross_width / 2.0), 0,
                          (s32)(combined_buffer.width / 2.0 + cross_width / 2.0), state->window_height, 0, 0,
                          (s32)cross_width, state->window_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, projection_buffer.id);
        glBlitFramebuffer((s32)(projection_buffer.width / 2.0 - projection_width / 2.0), 0,
                          (s32)(projection_buffer.width / 2.0 + projection_width / 2.0), state->window_height,
                          (s32)cross_width, 0, (s32)(cross_width + projection_width), state->window_height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Draw divider bar

        bind_default_framebuffer();
        glDisable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glViewport(0, 0, state->window_width, state->window_height);

        f64 divider_x_pos = (state->visualization_width * state->divider) * 2.0 - 1.0 - divider_width / 2.0;
        divider_bar_shader_prog.set_uniform_f32("x_pos", (f32)divider_x_pos);
        divider_bar_vao.draw();

    } else {
        glBlitFramebuffer(0, 0, combined_buffer.width, state->window_height, 0, 0, combined_buffer.width,
                          state->window_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    bind_default_framebuffer();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(state->window);

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

namespace mapbox {
namespace util {

template <>
struct nth<0, glm::dvec2> {
    static double get(const glm::dvec2& v) {
        return v.x;
    }
};

template <>
struct nth<1, glm::dvec2> {
    static double get(const glm::dvec2& v) {
        return v.y;
    }
};
} // namespace util
} // namespace mapbox
