#include <four/math.hpp>
#include <four/render.hpp>

#include <glad/glad.h>
#include <igl/triangle/triangulate.h>
#include <loguru.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace four {

namespace {

u32 compile_shader(const char* path, GLenum shader_type) {

    // FIXME: This is inefficient
    std::ifstream stream(path);
    std::stringstream source_buffer;
    source_buffer << stream.rdbuf();
    std::string source = source_buffer.str();
    const char* source_c = source.c_str();

    u32 shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source_c, nullptr);
    glCompileShader(shader);

    s32 success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    // FIXME: Return error message to caller
    if (!success) {
        s32 len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        std::vector<char> log((size_t)len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        ABORT_F("Shader compilation failed: %s", log.data());
    }

    return shader;
}
} // namespace

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

    u32 vert_shader = compile_shader("data/vertex.glsl", GL_VERTEX_SHADER);
    CHECK_NE_F(vert_shader, 0u);

    u32 frag_shader = compile_shader("data/fragment.glsl", GL_FRAGMENT_SHADER);
    CHECK_NE_F(frag_shader, 0u);

    shader_prog = glCreateProgram();
    glAttachShader(shader_prog, vert_shader);
    glAttachShader(shader_prog, frag_shader);
    glLinkProgram(shader_prog);

    {
        // Check for shader program linking errors
        s32 success;
        glGetProgramiv(shader_prog, GL_LINK_STATUS, &success);
        if (!success) {
            s32 len;
            glGetProgramiv(shader_prog, GL_INFO_LOG_LENGTH, &len);

            std::vector<char> log((size_t)len);
            glGetProgramInfoLog(shader_prog, len, nullptr, log.data());
            ABORT_F("Program linking failed: %s", log.data());
        }
    }

    u32 frag_shader_selected_cell = compile_shader("data/fragment-selected-cell.glsl", GL_FRAGMENT_SHADER);
    CHECK_NE_F(frag_shader_selected_cell, 0u);

    shader_prog_selected_cell = glCreateProgram();
    glAttachShader(shader_prog_selected_cell, vert_shader);
    glAttachShader(shader_prog_selected_cell, frag_shader_selected_cell);
    glLinkProgram(shader_prog_selected_cell);

    {
        // Check for shader program linking errors
        s32 success;
        glGetProgramiv(shader_prog_selected_cell, GL_LINK_STATUS, &success);
        if (!success) {
            s32 len;
            glGetProgramiv(shader_prog_selected_cell, GL_INFO_LOG_LENGTH, &len);

            std::vector<char> log((size_t)len);
            glGetProgramInfoLog(shader_prog_selected_cell, len, nullptr, log.data());
            ABORT_F("Program linking failed: %s", log.data());
        }
    }

    // Vertex array object for edges
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, s.mesh.vertices.size() * 3 * sizeof(f32), NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    u32 ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, s.mesh.edges.size() * sizeof(Edge), s.mesh.edges.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Vertex array object for the selected cell
    glGenVertexArrays(1, &vao_selected_cell);
    glBindVertexArray(vao_selected_cell);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ebo_selected_cell);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    vp_location = glGetUniformLocation(shader_prog, "vp");
    selected_cell_vp_location = glGetUniformLocation(shader_prog_selected_cell, "vp");

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
    triangulate(s.mesh, s.selected_cell, selected_cell_tri_faces);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        projected_vertices_f32.clear();
        for (const auto& v : projected_vertices) {
            for (f64 element : v.Elements) {
                projected_vertices_f32.push_back((f32)element);
            }
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, s.mesh.vertices.size() * 3 * sizeof(f32), projected_vertices_f32.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

        size_t selected_cell_tri_faces_size_b = selected_cell_tri_faces.size() * sizeof(u32);
        if (ebo_selected_cell_size < selected_cell_tri_faces_size_b) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, selected_cell_tri_faces_size_b, selected_cell_tri_faces.data(),
                         GL_STREAM_DRAW);
            ebo_selected_cell_size = selected_cell_tri_faces_size_b;
        } else {
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, selected_cell_tri_faces_size_b, selected_cell_tri_faces.data());
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    {
        hmm_mat4 view = HMM_LookAt(s.camera_pos, s.camera_target, s.camera_up);
        hmm_mat4 vp = projection * view;

        f32 vp_f32[16];
        for (s32 col = 0; col < 4; col++) {
            for (s32 row = 0; row < 4; row++) {
                vp_f32[col * 4 + row] = (f32)vp[col][row];
            }
        }

        glBindVertexArray(vao_selected_cell);
        glUseProgram(shader_prog_selected_cell);
        glUniformMatrix4fv(selected_cell_vp_location, 1, false, vp_f32);
        glDrawElements(GL_TRIANGLES, (GLsizei)selected_cell_tri_faces.size(), GL_UNSIGNED_INT, 0);

        glBindVertexArray(vao);
        glUseProgram(shader_prog);
        glUniformMatrix4fv(vp_location, 1, false, vp_f32);
        glDrawElements(GL_LINES, (GLsizei)(2 * s.mesh.edges.size()), GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
    }

    SDL_GL_SwapWindow(window);
}

void Renderer::triangulate(const Mesh4& mesh, u32 cell, std::vector<u32>& out_faces) {

    for (u32 face_i : mesh.cells[cell]) {
        const auto& face = mesh.faces[face_i];

        // Calculate normal vector

        const auto& edge0 = mesh.edges[face[0]];
        hmm_vec3 v0 = projected_vertices[edge0.v0];
        hmm_vec3 l0 = v0 - projected_vertices[edge0.v1];

        hmm_vec3 normal;
        for (size_t i = 0; i < face.size(); i++) {
            const auto& edge = mesh.edges[face[i]];
            hmm_vec3 l1 = projected_vertices[edge.v0] - projected_vertices[edge.v1];
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
            const auto& e = mesh.edges[edge_i];
            for (int i = 0; i < 2; i++) {
                u32 v_i = e.vertices[i];
                if (face2_vertex_i_mapping.left.find(v_i) == face2_vertex_i_mapping.left.end()) {
                    face2_vertex_i_mapping.left.insert(
                            VertexIMapping::left_value_type(v_i, (u32)face2_vertices.size()));
                    hmm_vec3 v_ = transform(to_2d_trans, projected_vertices[v_i]);
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
                hmm_vec3 v = projected_vertices[entry.first];
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
                out_faces.push_back(face2_vertex_i_mapping.right.at(triangulate_out_f(i, j)));
            }
        }
    }
}
} // namespace four
