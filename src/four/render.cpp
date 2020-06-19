#include <four/math.hpp>
#include <four/render.hpp>

#include <igl/triangle/triangulate.h>

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

u32 link_shader_program(u32 vertex_shader, u32 fragment_shader) {
    u32 program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    s32 success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        s32 len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log((size_t)len);
        glGetProgramInfoLog(program, len, NULL, log.data());
        ABORT_F("Program linking failed: %s", log.data());
    }

    return program;
}
} // namespace

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

void ElementBufferObject::buffer_data(const void* data, s32 n) {
    buf.buffer_data(data, (u32)n * sizeof(u32));
    primitive_count = n;
}

VertexArrayObject::VertexArrayObject(u32 shader_program, VertexBufferObject* vbo, VertexSpec spec,
                                     ElementBufferObject ebo)
        : shader_program(shader_program), vbo(vbo), ebo(ebo) {

    CHECK_EQ_F(vbo->type, (u32)GL_ARRAY_BUFFER);
    CHECK_EQ_F(ebo.buf.type, (u32)GL_ELEMENT_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    glBindBuffer(vbo->type, vbo->id);
    glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride, (void*)spec.offset);
    glEnableVertexAttribArray(spec.index);

    glBindBuffer(ebo.buf.type, ebo.buf.id);

    glBindVertexArray(0);
}

void VertexArrayObject::draw() {
    glUseProgram(shader_program);
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.primitive_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void VertexArrayObject::set_uniform_mat4(const char* name, const f32* data) {
    auto it = uniform_locations.find(name);
    s32 location;
    if (it == uniform_locations.end()) {
        location = glGetUniformLocation(shader_program, name);
        CHECK_NE_F(location, -1);
        uniform_locations.emplace(name, location);
    } else {
        location = it->second;
    }

    glUseProgram(shader_program);
    glUniformMatrix4fv(location, 1, false, data);
}

Renderer::Renderer(SDL_Window* window, const AppState* state) : window(window), state(state) {
    const auto& s = *state;

    SDL_GL_SetSwapInterval(1);

    s32 window_width, window_height;
    SDL_GL_GetDrawableSize(window, &window_width, &window_height);

    glViewport(0, 0, window_width, window_height);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0, -1.0);

    u32 wireframe_vert_shader = compile_shader("data/vertex.glsl", GL_VERTEX_SHADER);
    u32 wireframe_frag_shader = compile_shader("data/fragment.glsl", GL_FRAGMENT_SHADER);
    u32 wireframe_prog = link_shader_program(wireframe_vert_shader, wireframe_frag_shader);

    u32 cell_frag_shader = compile_shader("data/fragment-selected-cell.glsl", GL_FRAGMENT_SHADER);
    u32 cell_prog = link_shader_program(wireframe_vert_shader, cell_frag_shader);

    vertices = new_vertex_buffer_object(GL_STREAM_DRAW);
    VertexSpec vertices_spec = {
            .index = 0,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    ElementBufferObject wireframe_ebo(GL_STATIC_DRAW, GL_LINES);
    wireframe_ebo.buffer_data(s.mesh.edges.data(), 2 * (s32)s.mesh.edges.size());
    wireframe = VertexArrayObject(wireframe_prog, &vertices, vertices_spec, wireframe_ebo);

    ElementBufferObject cell_ebo(GL_STREAM_DRAW, GL_TRIANGLES);
    selected_cell = VertexArrayObject(cell_prog, &vertices, vertices_spec, cell_ebo);

    projection = HMM_Perspective(90, (f64)window_width / (f64)window_height, 0.1, 100.0);
}

void Renderer::render() {
    const auto& s = *state;

    // Perform 4D to 3D projection
    {
        projected_vertices.clear();
        Mat5 model = translate(s.mesh_pos);
        Mat5 view = look_at(s.camera4_pos, s.camera4_target, s.camera4_up, s.camera4_over);
        Mat5 mv = view * model;
        for (const hmm_vec4& v : s.mesh.vertices) {
            hmm_vec3 v_ = vec3(project_perspective(mv * vec5(v, 1), 1.0));
            projected_vertices.push_back(v_);
        }
        CHECK_EQ_F(s.mesh.vertices.size(), projected_vertices.size());
    }

    // Triangulate selected cell
    selected_cell_tri_faces.clear();
    for (u32 face_i : s.mesh.cells[s.selected_cell]) {
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

    vertices.buffer_data(projected_vertices_f32.data(), projected_vertices_f32.size() * 3 * sizeof(f32));
    selected_cell.ebo.buffer_data(selected_cell_tri_faces.data(), (s32)selected_cell_tri_faces.size());

    hmm_mat4 view = HMM_LookAt(s.camera_pos, s.camera_target, s.camera_up);
    hmm_mat4 vp = projection * view;

    f32 vp_f32[16];
    for (s32 col = 0; col < 4; col++) {
        for (s32 row = 0; row < 4; row++) {
            vp_f32[col * 4 + row] = (f32)vp[col][row];
        }
    }

    selected_cell.set_uniform_mat4("vp", vp_f32);
    selected_cell.draw();

    wireframe.set_uniform_mat4("vp", vp_f32);
    wireframe.draw();

    SDL_GL_SwapWindow(window);
}

void Renderer::triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges,
                           const std::vector<u32>& face, std::vector<u32>& out) {

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
    if (float_eq(std::abs(normal.Y), 1.0)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    hmm_mat4 to_2d_trans = HMM_LookAt(v0, v0 + normal, up);

    face2_vertex_i_mapping.clear();
    face2_vertices.clear();
    face2_edges.clear();

    for (u32 edge_i : face) {
        const auto& e = edges[edge_i];
        for (u32 v_i : e.vertices) {
            if (face2_vertex_i_mapping.left.find(v_i) == face2_vertex_i_mapping.left.end()) {
                face2_vertex_i_mapping.left.insert(VertexIMapping::left_value_type(v_i, (u32)face2_vertices.size()));
                hmm_vec3 v_ = transform(to_2d_trans, vertices[v_i]);
                CHECK_F(float_eq(v_.Z, 0.0));
                face2_vertices.push_back(vec2(v_));
            }
        }
        face2_edges.push_back(edge(face2_vertex_i_mapping.left.at(e.v0), face2_vertex_i_mapping.left.at(e.v1)));
    }

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : face2_vertex_i_mapping.left) {
        if (entry.first != edge0.v0) {
            hmm_vec3 v = vertices[entry.first];
            f64 x = HMM_Dot(v - v0, normal);

            // NOTE: Consider floating-point error. See
            // https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
            CHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    // `igl::triangle::triangulate()` only accepts double precision
    // floating point values.
    mesh_v.resize((s64)face2_vertices.size(), Eigen::NoChange);
    for (size_t i = 0; i < face2_vertices.size(); i++) {
        hmm_vec2 v = face2_vertices[i];
        mesh_v((s64)i, 0) = v.X;
        mesh_v((s64)i, 1) = v.Y;
    }

    mesh_e.resize((s64)face2_edges.size(), Eigen::NoChange);
    for (size_t i = 0; i < face2_edges.size(); i++) {
        Edge e = face2_edges[i];
        mesh_e((s64)i, 0) = (s32)e.v0;
        mesh_e((s64)i, 1) = (s32)e.v1;
    }

    // TODO: Support triangulating faces with holes
    const Eigen::MatrixX2d h;

    triangulate_out_v.resize(0, Eigen::NoChange);
    triangulate_out_f.resize(0, Eigen::NoChange);
    igl::triangle::triangulate(mesh_v, mesh_e, h, "Q", triangulate_out_v, triangulate_out_f);

    CHECK_EQ_F((size_t)triangulate_out_v.rows(), face2_vertices.size());

#ifdef FOUR_DEBUG
    for (s32 i = 0; i < triangulate_out_v.rows(); i++) {
        CHECK_F(float_eq(triangulate_out_v(i, 0), face2_vertices[(size_t)i].X));
        CHECK_F(float_eq(triangulate_out_v(i, 1), face2_vertices[(size_t)i].Y));
    }
#endif

    for (s32 i = 0; i < triangulate_out_f.rows(); i++) {
        for (s32 j = 0; j < 3; j++) {
            out.push_back(face2_vertex_i_mapping.right.at(triangulate_out_f(i, j)));
        }
    }
}
} // namespace four
