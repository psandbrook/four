#include <SDL.h>
#include <glad/glad.h>

#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace {

class SdlGuard {
public:
    ~SdlGuard() {
        SDL_Quit();
    }
};

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

    SdlGuard sdl_guard;

    // FIXME: Check these function calls for errors
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    int window_width = 1280;
    int window_height = 720;

    SDL_Window* window = SDL_CreateWindow("4dtest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width,
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

    glViewport(0, 0, window_width, window_height);

    float vertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };

    uint32_t indices[] = {0, 1, 2};

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

    uint32_t vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    uint32_t vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    uint32_t ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

    double count_per_ms = (double)SDL_GetPerformanceFrequency() / 1000.0;
    int steps_per_sec = 60;
    double step_ms = 1000.0 / steps_per_sec;

    double lag_ms = 0.0;
    uint64_t last_count = SDL_GetPerformanceCounter();

    // main_loop
    while (true) {

        uint64_t new_count = SDL_GetPerformanceCounter();
        double elapsed_ms = (double)(new_count - last_count) / count_per_ms;
        last_count = new_count;
        lag_ms += elapsed_ms;

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

        int steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {
            lag_ms -= step_ms;
            steps++;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
        SDL_GL_SwapWindow(window);
    }
main_loop_end:

    return 0;
}
