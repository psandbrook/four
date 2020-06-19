#include <four/generate.hpp>
#include <four/math.hpp>
#include <four/mesh.hpp>
#include <four/utility.hpp>

#include <Eigen/Core>
#include <HandmadeMath.h>
#include <SDL.h>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <glad/glad.h>
#include <igl/triangle/triangulate.h>
#include <loguru.hpp>

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

        std::vector<char> log;
        log.reserve((size_t)len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        fprintf(stderr, "Shader compilation failed: %s\n", log.data());
        return 0;
    }

    return shader;
}
} // namespace

int main(int argc, char** argv) {
    loguru::init(argc, argv);

    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
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

    s32 window_width, window_height;
    SDL_GL_GetDrawableSize(window, &window_width, &window_height);

    // Initial OpenGL calls

    glViewport(0, 0, window_width, window_height);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0);

    // This is needed to render the wireframe without z-fighting.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0, -1.0);

    u32 vert_shader = compile_shader("data/vertex.glsl", GL_VERTEX_SHADER);
    if (!vert_shader) {
        return 1;
    }

    u32 frag_shader = compile_shader("data/fragment.glsl", GL_FRAGMENT_SHADER);
    if (!frag_shader) {
        return 1;
    }

    u32 shader_prog = glCreateProgram();
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

            std::vector<char> log;
            log.reserve((size_t)len);
            glGetProgramInfoLog(shader_prog, len, nullptr, log.data());
            fprintf(stderr, "Program linking failed: %s\n", log.data());
            return 1;
        }
    }

    u32 frag_shader_selected_cell = compile_shader("data/fragment-selected-cell.glsl", GL_FRAGMENT_SHADER);
    if (!frag_shader_selected_cell) {
        return 1;
    }

    u32 shader_prog_selected_cell = glCreateProgram();
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

            std::vector<char> log;
            log.reserve((size_t)len);
            glGetProgramInfoLog(shader_prog_selected_cell, len, nullptr, log.data());
            fprintf(stderr, "Program linking failed: %s\n", log.data());
            return 1;
        }
    }

    {
        Mesh4 mesh = generate_120cell();
        CHECK_F(save_mesh_to_file(mesh, "120cell.mesh4"));
    }

    Mesh4 mesh = load_mesh_from_file("120cell.mesh4");
    hmm_vec4 mesh_pos = {0, 0, 0, 2.5};
    s32 selected_cell = 0;

    // Vertex array object for edges
    u32 vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * 3 * sizeof(f32), NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    u32 ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.edges.size() * sizeof(Edge), mesh.edges.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Vertex array object for the selected cell
    u32 vao_selected_cell;
    glGenVertexArrays(1, &vao_selected_cell);
    glBindVertexArray(vao_selected_cell);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    u32 ebo_selected_cell;
    size_t ebo_selected_cell_size = 0;
    glGenBuffers(1, &ebo_selected_cell);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    s32 vp_location = glGetUniformLocation(shader_prog, "vp");
    s32 selected_cell_vp_location = glGetUniformLocation(shader_prog_selected_cell, "vp");

    hmm_vec4 camera4_pos = {0, 0, 0, 0};
    hmm_vec4 camera4_target = {0, 0, 0, 1};
    hmm_vec4 camera4_up = {0, 1, 0, 0};
    hmm_vec4 camera4_over = {0, 0, 1, 0};

    hmm_vec3 camera_pos = {0, 0, 4};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

    const f64 camera_move_units_per_sec = 1.0;
    bool camera_move_up = false;
    bool camera_move_down = false;
    bool camera_move_forward = false;
    bool camera_move_backward = false;
    bool camera_move_left = false;
    bool camera_move_right = false;

    hmm_mat4 projection = HMM_Perspective(90, (f64)window_width / (f64)window_height, 0.1, 100.0);

    const f64 count_per_ms = (f64)SDL_GetPerformanceFrequency() / 1000.0;
    const s32 steps_per_sec = 60;
    const f64 step_ms = 1000.0 / steps_per_sec;

    f64 lag_ms = 0.0;
    u64 last_count = SDL_GetPerformanceCounter();
    f64 second_acc = 0.0;
    s32 frames = 0;

    std::vector<hmm_vec3> projected_vertices;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    // main_loop
    while (true) {
        u64 new_count = SDL_GetPerformanceCounter();
        f64 elapsed_ms = (f64)(new_count - last_count) / count_per_ms;
        last_count = new_count;
        lag_ms += elapsed_ms;
        second_acc += elapsed_ms;

        if (second_acc >= 1000.0) {
            LOG_F(INFO, "fps: %i", frames);
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
                f64 x_angle = 0.2 * (f64)event.motion.xrel;
                camera_pos = transform(rotate(camera_target, x_angle, camera_up), camera_pos);

                f64 y_angle = 0.2 * (f64)-event.motion.yrel;
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 new_camera_pos = transform(rotate(camera_target, y_angle, left), camera_pos);
                hmm_vec3 front = HMM_Normalize(camera_target - new_camera_pos);
                if (!float_eq(std::abs(front.Y), 1.0, 0.001)) {
                    camera_pos = new_camera_pos;
                }
            } break;
            }
        }

        // Run simulation

        s32 steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {

            f64 camera_move_units = camera_move_units_per_sec * ((f64)step_ms / 1000.0);

            if (camera_move_up) {
                hmm_mat4 m_t = HMM_Translate(camera_move_units * camera_up);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            if (camera_move_down) {
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * camera_up);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            if (camera_move_forward) {
                hmm_vec3 front = camera_target - camera_pos;
                hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
                hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            if (camera_move_backward) {
                hmm_vec3 front = camera_target - camera_pos;
                hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            if (camera_move_left) {
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
                hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            if (camera_move_right) {
                hmm_vec3 left = HMM_Cross(camera_up, camera_target - camera_pos);
                hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
                hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
                camera_pos = transform(m_t, camera_pos);
                camera_target = transform(m_t, camera_target);
            }

            Rotor4 r = rotor4({1, 0, 0, 0}, {1, 0, 0, 0.002});

            for (size_t i = 0; i < mesh.vertices.size(); i++) {
                mesh.vertices[i] = rotate(r, mesh.vertices[i]);
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
                hmm_vec3 v_ = vec3(project_perspective(mv * vec5(v, 1), 1.0));
                projected_vertices.push_back(v_);
            }
            assert(mesh.vertices.size() == projected_vertices.size());
        }

        // Triangulate selected cell
        std::vector<u32> triangulate_faces;

        for (u32 face_i : mesh.cells[(size_t)selected_cell]) {
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
                    assert(i < face.size() - 1);
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

            // std::unordered_map<u32, u32> face2_vertex_i_mapping;

            using MappingT = boost::bimap<boost::bimaps::unordered_set_of<u32>, boost::bimaps::unordered_set_of<u32>>;
            MappingT face2_vertex_i_mapping;

            std::vector<hmm_vec2> face2_vertices;
            std::vector<Edge> face2_edges;

            for (u32 edge_i : face) {
                const auto& edge = mesh.edges[edge_i];
                for (int i = 0; i < 2; i++) {
                    u32 v_i = edge.vertices[i];
                    if (face2_vertex_i_mapping.left.find(v_i) == face2_vertex_i_mapping.left.end()) {
                        face2_vertex_i_mapping.left.insert(MappingT::left_value_type(v_i, (u32)face2_vertices.size()));
                        hmm_vec3 v_ = transform(to_2d_trans, projected_vertices[v_i]);
                        assert(float_eq(v_.Z, 0.0));
                        face2_vertices.push_back(vec2(v_));
                    }
                }
                face2_edges.push_back(
                        {face2_vertex_i_mapping.left.at(edge.v0), face2_vertex_i_mapping.left.at(edge.v1)});
            }

#ifdef FOUR_DEBUG
            // All vertices should be coplanar
            for (const auto& entry : face2_vertex_i_mapping.left) {
                if (entry.first != edge0.v0) {
                    hmm_vec3 v = projected_vertices[entry.first];
                    f64 x = HMM_Dot(v - v0, normal);

                    // FIXME: Floating-point error!
                    // See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
                    assert(float_eq(x, 0.0));
                }
            }
#endif

            // `igl::triangle::triangulate()` only accepts double precision
            // floating point values.
            Eigen::MatrixX2d mesh_v(face2_vertices.size(), 2);
            for (size_t i = 0; i < face2_vertices.size(); i++) {
                hmm_vec2 v = face2_vertices[i];
                mesh_v((s64)i, 0) = v.X;
                mesh_v((s64)i, 1) = v.Y;
            }

            Eigen::MatrixX2i mesh_e(face2_edges.size(), 2);
            for (size_t i = 0; i < face2_edges.size(); i++) {
                Edge e = face2_edges[i];
                mesh_e((s64)i, 0) = (s32)e.v0;
                mesh_e((s64)i, 1) = (s32)e.v1;
            }

            Eigen::MatrixX2d h;

            Eigen::MatrixX2d triangulate_out_v;
            Eigen::MatrixX3i triangulate_out_f;

            // The triangulation algorithm may add new vertices. This means that
            // adjacent faces of the polyhedron no longer share vertices.
            //
            // FIXME: Use flags "Q" to ensure that no edges are subdivided!
            igl::triangle::triangulate(mesh_v, mesh_e, h, "Q", triangulate_out_v, triangulate_out_f);

            assert((size_t)triangulate_out_v.rows() == face2_vertices.size());

#ifdef FOUR_DEBUG
            for (s32 i = 0; i < triangulate_out_v.rows(); i++) {
                assert(float_eq(triangulate_out_v(i, 0), face2_vertices[(size_t)i].X));
                assert(float_eq(triangulate_out_v(i, 1), face2_vertices[(size_t)i].Y));
            }
#endif

            for (s32 i = 0; i < triangulate_out_f.rows(); i++) {
                for (s32 j = 0; j < 3; j++) {
                    triangulate_faces.push_back(face2_vertex_i_mapping.right.at(triangulate_out_f(i, j)));
                }
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            std::vector<f32> projected_vertices_f32;
            for (const auto& v : projected_vertices) {
                for (f64 element : v.Elements) {
                    projected_vertices_f32.push_back((f32)element);
                }
            }
            glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.vertices.size() * 3 * sizeof(f32), projected_vertices_f32.data());

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_selected_cell);

            size_t triangulate_faces_size_b = triangulate_faces.size() * sizeof(u32);
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

            f32 vp_f32[16];
            for (s32 col = 0; col < 4; col++) {
                for (s32 row = 0; row < 4; row++) {
                    vp_f32[col * 4 + row] = (f32)vp[col][row];
                }
            }

            glBindVertexArray(vao_selected_cell);
            glUseProgram(shader_prog_selected_cell);
            glUniformMatrix4fv(selected_cell_vp_location, 1, false, vp_f32);
            glDrawElements(GL_TRIANGLES, (GLsizei)triangulate_faces.size(), GL_UNSIGNED_INT, 0);

            glBindVertexArray(vao);
            glUseProgram(shader_prog);
            glUniformMatrix4fv(vp_location, 1, false, vp_f32);
            glDrawElements(GL_LINES, (GLsizei)(2 * mesh.edges.size()), GL_UNSIGNED_INT, 0);

            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(window);
        frames++;
    }
main_loop_end:

    return 0;
}
