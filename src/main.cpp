#include <four/generate.hpp>
#include <four/math.hpp>
#include <four/mesh.hpp>

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

#include <Eigen/Core>
#include <SDL.h>
#include <glad/glad.h>
#include <igl/triangle/triangulate.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace four;

namespace {

const float look_at_y_epsilon = 0.0001f;

uint32_t compile_shader(const char* path, GLenum shader_type) {

    // FIXME: This is inefficient
    std::ifstream stream(path);
    std::stringstream source_buffer;
    source_buffer << stream.rdbuf();
    std::string source = source_buffer.str();
    const char* source_c = source.c_str();

    uint32_t shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source_c, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    // FIXME: Return error message to caller
    if (!success) {
        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        std::vector<char> log;
        log.reserve(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        fprintf(stderr, "Shader compilation failed: %s\n", log.data());
        return 0;
    }

    return shader;
}
} // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    struct SdlGuard {
        ~SdlGuard() {
            SDL_Quit();
        }
    } sdl_guard;

    // FIXME: Check these calls for errors
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    // Enable multisampling
#if 0
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#endif

    SDL_Window* window = SDL_CreateWindow("four", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        return 1;
    }

    if (!SDL_GL_CreateContext(window)) {
        return 1;
    }

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    int window_width, window_height;
    SDL_GL_GetDrawableSize(window, &window_width, &window_height);

    // Initial OpenGL calls

    glViewport(0, 0, window_width, window_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);

    uint32_t vert_shader = compile_shader("data/vertex.glsl", GL_VERTEX_SHADER);
    if (!vert_shader) {
        return 1;
    }

    uint32_t frag_shader = compile_shader("data/fragment.glsl", GL_FRAGMENT_SHADER);
    if (!frag_shader) {
        return 1;
    }

    uint32_t shader_prog = glCreateProgram();
    glAttachShader(shader_prog, vert_shader);
    glAttachShader(shader_prog, frag_shader);
    glLinkProgram(shader_prog);

    {
        // Check for shader program linking errors
        int success;
        glGetProgramiv(shader_prog, GL_LINK_STATUS, &success);
        if (!success) {
            int len;
            glGetProgramiv(shader_prog, GL_INFO_LOG_LENGTH, &len);

            std::vector<char> log;
            log.reserve(len);
            glGetProgramInfoLog(shader_prog, len, nullptr, log.data());
            fprintf(stderr, "Program linking failed: %s\n", log.data());
            return 1;
        }
    }

    uint32_t frag_shader_selected_cell = compile_shader("data/fragment-selected-cell.glsl", GL_FRAGMENT_SHADER);
    if (!frag_shader_selected_cell) {
        return 1;
    }

    uint32_t shader_prog_selected_cell = glCreateProgram();
    glAttachShader(shader_prog_selected_cell, vert_shader);
    glAttachShader(shader_prog_selected_cell, frag_shader_selected_cell);
    glLinkProgram(shader_prog_selected_cell);

    {
        // Check for shader program linking errors
        int success;
        glGetProgramiv(shader_prog_selected_cell, GL_LINK_STATUS, &success);
        if (!success) {
            int len;
            glGetProgramiv(shader_prog_selected_cell, GL_INFO_LOG_LENGTH, &len);

            std::vector<char> log;
            log.reserve(len);
            glGetProgramInfoLog(shader_prog_selected_cell, len, nullptr, log.data());
            fprintf(stderr, "Program linking failed: %s\n", log.data());
            return 1;
        }
    }

    Mesh4 mesh = generate_tesseract();
    hmm_vec4 mesh_pos = {0, 0, 0, 2};
    printf("%lu %lu %lu %lu\n", mesh.vertices.size(), mesh.edges.size(), mesh.faces.size(), mesh.cells.size());

    int selected_cell = 0;

    // Vertex array object for edges
    uint32_t vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    uint32_t vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(hmm_vec3), NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(hmm_vec3), (void*)0);
    glEnableVertexAttribArray(0);

    uint32_t ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.edges.size() * sizeof(Edge), mesh.edges.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Vertex array object for the selected cell
    uint32_t vao_selected_cell;
    glGenVertexArrays(1, &vao_selected_cell);
    glBindVertexArray(vao_selected_cell);

    uint32_t vbo_selected_cell;
    size_t vbo_selected_cell_size = 0;
    glGenBuffers(1, &vbo_selected_cell);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_selected_cell);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(hmm_vec3), (void*)0);
    glEnableVertexAttribArray(0);

    uint32_t ebo_selected_cell;
    size_t ebo_selected_cell_size = 0;
    glGenBuffers(1, &ebo_selected_cell);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    int vp_location = glGetUniformLocation(shader_prog, "vp");
    int selected_cell_vp_location = glGetUniformLocation(shader_prog_selected_cell, "vp");

    hmm_vec4 camera4_pos = {0, 0, 0, 0};
    hmm_vec4 camera4_target = {0, 0, 0, 1};
    hmm_vec4 camera4_up = {0, 1, 0, 0};
    hmm_vec4 camera4_over = {0, 0, 1, 0};

    hmm_vec3 camera_pos = {0, 0, 4};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

    const float camera_move_units_per_sec = 1.0f;
    bool camera_move_up = false;
    bool camera_move_down = false;
    bool camera_move_forward = false;
    bool camera_move_backward = false;
    bool camera_move_left = false;
    bool camera_move_right = false;

    double selected_cell_counter = 0.0;

    hmm_mat4 projection = HMM_Perspective(90, (float)window_width / (float)window_height, 0.1f, 100.0f);

    const double count_per_ms = (double)SDL_GetPerformanceFrequency() / 1000.0;
    const int steps_per_sec = 60;
    const double step_ms = 1000.0 / steps_per_sec;

    double lag_ms = 0.0;
    uint64_t last_count = SDL_GetPerformanceCounter();
    double second_acc = 0.0;
    int frames = 0;

    std::vector<hmm_vec3> projected_vertices;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    // main_loop
    while (true) {
        uint64_t new_count = SDL_GetPerformanceCounter();
        double elapsed_ms = (double)(new_count - last_count) / count_per_ms;
        last_count = new_count;
        lag_ms += elapsed_ms;
        second_acc += elapsed_ms;

        if (second_acc >= 1000.0) {
            printf("fps: %i\n", frames);
            second_acc = 0.0;
            frames = 0;
        }

        // Process input

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                goto main_loop_end;

            case SDL_KEYDOWN: {
                if (!event.key.repeat) {
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        goto main_loop_end;

                    case SDLK_r: {
                        camera_move_up = true;
                    } break;

                    case SDLK_f: {
                        camera_move_down = true;
                    } break;

                    case SDLK_w: {
                        camera_move_forward = true;
                    } break;

                    case SDLK_s: {
                        camera_move_backward = true;
                    } break;

                    case SDLK_a: {
                        camera_move_left = true;
                    } break;

                    case SDLK_d: {
                        camera_move_right = true;
                    } break;
                    }
                }
            } break;

            case SDL_KEYUP: {
                if (!event.key.repeat) {
                    switch (event.key.keysym.sym) {
                    case SDLK_r: {
                        camera_move_up = false;
                    } break;

                    case SDLK_f: {
                        camera_move_down = false;
                    } break;

                    case SDLK_w: {
                        camera_move_forward = false;
                    } break;

                    case SDLK_s: {
                        camera_move_backward = false;
                    } break;

                    case SDLK_a: {
                        camera_move_left = false;
                    } break;

                    case SDLK_d: {
                        camera_move_right = false;
                    } break;
                    }
                }
            } break;

            case SDL_MOUSEMOTION: {
                // FIXME: Use quaternions?
                float x_angle = 0.2f * (float)event.motion.xrel;
                camera_pos = transform(camera_pos, rotate(camera_target, x_angle, camera_up));

                float y_angle = 0.2f * (float)-event.motion.yrel;
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 new_camera_pos = transform(camera_pos, rotate(camera_target, y_angle, left));
                hmm_vec3 front = HMM_Normalize(camera_target - new_camera_pos);
                if (!float_eq(std::abs(front.Y), 1.0f, look_at_y_epsilon)) {
                    camera_pos = new_camera_pos;
                }
            } break;
            }
        }

        // Run simulation

        int steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {

            float camera_move_units = camera_move_units_per_sec * ((float)step_ms / 1000.0f);

            if (camera_move_up) {
                hmm_mat4 m_t = HMM_Translate(camera_move_units * camera_up);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            if (camera_move_down) {
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * camera_up);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            if (camera_move_forward) {
                hmm_vec3 front = camera_target - camera_pos;
                hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
                hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            if (camera_move_backward) {
                hmm_vec3 front = camera_target - camera_pos;
                hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            if (camera_move_left) {
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
                hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            if (camera_move_right) {
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
                camera_pos = transform(camera_pos, m_t);
                camera_target = transform(camera_target, m_t);
            }

            selected_cell_counter += step_ms;
            if (selected_cell_counter >= 2000.0) {
                selected_cell++;
                if ((size_t)selected_cell == mesh.cells.size()) {
                    selected_cell = 0;
                }
                selected_cell_counter = 0.0;
            }

            lag_ms -= step_ms;
            steps++;
        }

        // Render

        // Perform 4D to 3D projection
        {
            projected_vertices.clear();
            Mat5 model = translate(mesh_pos);
            Mat5 view = look_at(camera4_pos, camera4_target, camera4_up, camera4_over);
            Mat5 mv = view * model;
            for (const hmm_vec4& v : mesh.vertices) {
                hmm_vec3 v_ = vec3(project_perspective(mv * vec5(v, 1), 1.0f));
                projected_vertices.push_back(v_);
            }
            assert(mesh.vertices.size() == projected_vertices.size());
        }

        // Triangulate selected cell
        std::vector<hmm_vec3> triangulate_vertices;
        std::vector<uint32_t> triangulate_faces;

        for (uint32_t face_i : mesh.cells[selected_cell]) {
            const auto& face = mesh.faces[face_i];

            // Calculate normal vector

            const auto& edge0 = mesh.edges[face[0]];
            hmm_vec3 v0 = projected_vertices[edge0.v1];
            hmm_vec3 l0 = v0 - projected_vertices[edge0.v2];
            hmm_vec3 normal;
            for (size_t i = 0; i < face.size(); i++) {
                const auto& edge = mesh.edges[face[i]];
                hmm_vec3 l1 = projected_vertices[edge.v1] - projected_vertices[edge.v2];
                normal = HMM_Cross(l0, l1);
                if (float_eq_abs(normal.X, 0.0f) && float_eq_abs(normal.Y, 0.0f) && float_eq_abs(normal.Z, 0.0f)) {
                    // `normal` is the zero vector

                    // Fail if there are no more edges---this means the face has
                    // no surface area
                    assert(i < face.size() - 1);
                } else {
                    break;
                }
            }

            normal = HMM_Normalize(normal);

            // Calculate transformation to 2D and its inverse

            hmm_mat4 to_2d_trans;
            hmm_mat4 to_2d_trans_inverse;
            if (float_eq(std::abs(normal.Y), 1.0f, look_at_y_epsilon)) {
                hmm_mat4 m_r = HMM_Rotate(90.0f, {1, 0, 0});
                hmm_vec3 v0_r = transform(v0, m_r);
                hmm_vec3 normal_r = transform(normal, m_r);
                to_2d_trans = HMM_LookAt(v0_r, v0_r + normal_r, {0, 1, 0}) * m_r;

                hmm_mat4 m_r_inverse = HMM_Rotate(-90.0f, {1, 0, 0});
                to_2d_trans_inverse = m_r_inverse * look_at_inverse(v0_r, v0_r + normal_r, {0, 1, 0});
            } else {
                to_2d_trans = HMM_LookAt(v0, v0 + normal, {0, 1, 0});
                to_2d_trans_inverse = look_at_inverse(v0, v0 + normal, {0, 1, 0});
            }

#ifndef NDEBUG
            // Verify that `to_2d_trans_inverse` is the inverse of `to_2d_trans`
            hmm_vec4 v0_4 = HMM_Vec4v(v0, 1);
            hmm_vec4 v0_4_ = to_2d_trans_inverse * to_2d_trans * v0_4;
            for (int i = 0; i < 4; i++) {
                // FIXME: Floating-point error!
                assert(float_eq_abs(v0_4[i], v0_4_[i]));
            }
#endif

            std::unordered_map<uint32_t, uint32_t> face2_vertex_i_mapping;
            std::vector<hmm_vec2> face2_vertices;
            std::vector<Edge> face2_edges;

            for (uint32_t edge_i : face) {
                const auto& edge = mesh.edges[edge_i];
                if (!has_key(face2_vertex_i_mapping, edge.v1)) {
                    face2_vertex_i_mapping.emplace(edge.v1, face2_vertices.size());
                    hmm_vec3 v1_ = vec3(to_2d_trans * HMM_Vec4v(projected_vertices[edge.v1], 1));
                    assert(float_eq_abs(v1_.Z, 0.0f));
                    face2_vertices.push_back(vec2(v1_));
                }
                if (!has_key(face2_vertex_i_mapping, edge.v2)) {
                    face2_vertex_i_mapping.emplace(edge.v2, face2_vertices.size());
                    hmm_vec3 v2_ = vec3(to_2d_trans * HMM_Vec4v(projected_vertices[edge.v2], 1));
                    assert(float_eq_abs(v2_.Z, 0.0f));
                    face2_vertices.push_back(vec2(v2_));
                }
                face2_edges.push_back({face2_vertex_i_mapping[edge.v1], face2_vertex_i_mapping[edge.v2]});
            }

#ifndef NDEBUG
            // All vertices should be coplanar
            for (const auto& entry : face2_vertex_i_mapping) {
                if (entry.first != edge0.v1) {
                    hmm_vec3 v = projected_vertices[entry.first];
                    float x = HMM_Dot(v - v0, normal);

                    // FIXME: Floating-point error!
                    // See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
                    assert(float_eq_abs(x, 0.0f));
                }
            }
#endif

#if 0
            if (face_i == 6) {
                fprintf(stderr, "normal: %.38f, %.38f, %.38f\n", normal.X, normal.Y, normal.Z);

                for (const auto& entry : face2_vertex_i_mapping) {
                    hmm_vec3 v = projected_vertices[entry.first];
                    fprintf(stderr, "vertex %u: %f, %f, %f\n", entry.first, v.X, v.Y, v.Z);
                }

                for (uint32_t edge_i : face) {
                    const auto& edge = mesh.edges[edge_i];
                    fprintf(stderr, "edge: %u %u\n", edge.v1, edge.v2);
                }

                for (int i = 0; i < face2_vertices.size(); i++) {
                    hmm_vec2 v = face2_vertices[i];
                    fprintf(stderr, "vertex %i: %f, %f\n", i, v.X, v.Y);
                }

                for (int i = 0; i < face2_edges.size(); i++) {
                    Edge e = face2_edges[i];
                    fprintf(stderr, "edge: %u %u\n", e.v1, e.v2);
                }
            }
#endif

            // `igl::triangle::triangulate()` only accepts double-precision
            // floating point values.
            Eigen::MatrixX2d mesh_v(face2_vertices.size(), 2);
            for (size_t i = 0; i < face2_vertices.size(); i++) {
                hmm_vec2 v = face2_vertices[i];
                mesh_v(i, 0) = v.X;
                mesh_v(i, 1) = v.Y;
            }

            Eigen::MatrixX2i mesh_e(face2_edges.size(), 2);
            for (size_t i = 0; i < face2_edges.size(); i++) {
                Edge e = face2_edges[i];
                mesh_e(i, 0) = e.v1;
                mesh_e(i, 1) = e.v2;
            }

            Eigen::MatrixX2d h;

            Eigen::MatrixX2d triangulate_out_v;
            Eigen::MatrixX3i triangulate_out_f;

            // The triangulation algorithm may add new vertices. This means that
            // adjacent faces of the polyhedron no longer share vertices.
            //
            // FIXME: Use flags "Q" to ensure that no edges are subdivided!
            igl::triangle::triangulate(mesh_v, mesh_e, h, "q0Q", triangulate_out_v, triangulate_out_f);

            std::unordered_map<int, uint32_t> triangulate_vertex_i_mapping;
            for (int i = 0; i < triangulate_out_f.rows(); i++) {
                for (int j = 0; j < 3; j++) {
                    int v_i = triangulate_out_f(i, j);
                    if (!has_key(triangulate_vertex_i_mapping, v_i)) {
                        triangulate_vertex_i_mapping.emplace(v_i, triangulate_vertices.size());
                        hmm_vec3 v = {(float)triangulate_out_v(v_i, 0), (float)triangulate_out_v(v_i, 1), 0};
                        hmm_vec3 v_ = vec3(to_2d_trans_inverse * HMM_Vec4v(v, 1));
                        triangulate_vertices.push_back(v_);
                    }
                    triangulate_faces.push_back(triangulate_vertex_i_mapping[v_i]);
                }
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.vertices.size() * sizeof(hmm_vec3), projected_vertices.data());

            glBindBuffer(GL_ARRAY_BUFFER, vbo_selected_cell);

            // Reallocate the VBO only if it is not big enough
            size_t triangulate_vertices_size_b = triangulate_vertices.size() * sizeof(hmm_vec3);
            if (vbo_selected_cell_size < triangulate_vertices_size_b) {
                glBufferData(GL_ARRAY_BUFFER, triangulate_vertices_size_b, triangulate_vertices.data(), GL_STREAM_DRAW);
                vbo_selected_cell_size = triangulate_vertices_size_b;
            } else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, triangulate_vertices_size_b, triangulate_vertices.data());
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

            size_t triangulate_faces_size_b = triangulate_faces.size() * sizeof(uint32_t);
            if (ebo_selected_cell_size < triangulate_faces_size_b) {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangulate_faces_size_b, triangulate_faces.data(),
                             GL_STREAM_DRAW);
                ebo_selected_cell_size = triangulate_faces_size_b;
            } else {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, triangulate_faces_size_b, triangulate_faces.data());
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        {
            hmm_mat4 view = HMM_LookAt(camera_pos, camera_target, camera_up);
            hmm_mat4 vp = projection * view;

            glBindVertexArray(vao_selected_cell);
            glUseProgram(shader_prog_selected_cell);
            glUniformMatrix4fv(selected_cell_vp_location, 1, false, (float*)&vp);
            glDrawElements(GL_TRIANGLES, (GLsizei)triangulate_faces.size(), GL_UNSIGNED_INT, 0);

            glBindVertexArray(vao);
            glUseProgram(shader_prog);
            glUniformMatrix4fv(vp_location, 1, false, (float*)&vp);
            glDrawElements(GL_LINES, (GLsizei)(2 * mesh.edges.size()), GL_UNSIGNED_INT, 0);

            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(window);
        frames++;
    }
main_loop_end:

    return 0;
}
