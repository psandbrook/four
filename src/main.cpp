#include <four/generate.hpp>
#include <four/math.hpp>
#include <four/mesh.hpp>

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

#include <SDL.h>
#include <glad/glad.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace four;

namespace {

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
    /*
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    */

    const int window_width = 1280;
    const int window_height = 720;

    SDL_Window* window = SDL_CreateWindow("four", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width,
                                          window_height, SDL_WINDOW_OPENGL);
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

    // Initial OpenGL calls

    glViewport(0, 0, window_width, window_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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

    glUseProgram(shader_prog);

    Mesh4 mesh = generate_5cell();
    hmm_vec4 mesh_pos = {0, 0, 0, 0};
    printf("%lu %lu %lu %lu\n", mesh.vertices.size(), mesh.edges.size(), mesh.faces.size(), mesh.cells.size());

    uint32_t vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    uint32_t vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(hmm_vec3), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(hmm_vec3), (void*)0);
    glEnableVertexAttribArray(0);

    uint32_t ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * mesh.edges.size() * sizeof(uint32_t), mesh.edges.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    int vp_location = glGetUniformLocation(shader_prog, "vp");

    hmm_vec4 camera4_pos = {4, 4, 4, 4};
    hmm_vec4 camera4_target = {0, 0, 0, 0};
    hmm_vec4 camera4_up = {0, 1, 0, 0};
    hmm_vec4 camera4_over = {0, 0, 1, 0};

    Mat5 projection4 = mat5({1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 1});

    hmm_vec3 camera_pos = {0, 0, 4};
    hmm_vec3 camera_target = {0, 0, 0};
    hmm_vec3 camera_up = {0, 1, 0};

    hmm_mat4 projection = HMM_Perspective(75, window_width / (float)window_height, 0.1f, 100.0f);

    const double count_per_ms = (double)SDL_GetPerformanceFrequency() / 1000.0;
    const int steps_per_sec = 60;
    const double step_ms = 1000.0 / steps_per_sec;

    double lag_ms = 0.0;
    uint64_t last_count = SDL_GetPerformanceCounter();
    double second_acc = 0.0;
    int frames = 0;

    std::vector<hmm_vec3> projected_vertices;

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
                    }
                }
            } break;
            }
        }

        // Run simulation

        int steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {
            lag_ms -= step_ms;
            steps++;
        }

        // Render

        // Perform 4D to 3D projection
        {
            projected_vertices.clear();
            Mat5 model = translate(mesh_pos);
            Mat5 view = look_at(camera4_pos, camera4_target, camera4_up, camera4_over);
            Mat5 mvp = projection4 * view * model;
            for (const hmm_vec4& v : mesh.vertices) {
                hmm_vec3 v_ = vec3(vec4(mvp * vec5(v, 1)));
                projected_vertices.push_back(v_);
            }
            assert(mesh.vertices.size() == projected_vertices.size());
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.vertices.size() * sizeof(hmm_vec3), projected_vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        {
            hmm_mat4 view = HMM_LookAt(camera_pos, camera_target, camera_up);
            hmm_mat4 vp = projection * view;
            glUniformMatrix4fv(vp_location, 1, false, (float*)&vp);
        }

        glBindVertexArray(vao);
        glDrawElements(GL_LINES, (GLsizei)(2 * mesh.edges.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        SDL_GL_SwapWindow(window);
        frames++;
    }
main_loop_end:

    return 0;
}
