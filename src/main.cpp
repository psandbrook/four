#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

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

struct Vertex {
    hmm_vec3 pos;
    hmm_vec3 norm;
};

struct Face {
    Vertex vs[3];
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

void fill_normal(Face& face) {
    hmm_vec3 normal = HMM_Cross(face.vs[1].pos - face.vs[0].pos, face.vs[2].pos - face.vs[0].pos);
    face.vs[0].norm = normal;
    face.vs[1].norm = normal;
    face.vs[2].norm = normal;
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    glPointSize(5.0f);

    // clang-format off
    hmm_vec4 v0 = { 1.0f, 0.0f, 0.0f, 0.0f};
    hmm_vec4 v1 = { 0.0f, 1.0f, 0.0f, 0.0f};
    hmm_vec4 v2 = { 0.0f, 0.0f, 1.0f, 0.0f};
    hmm_vec4 v3 = {-1.0f, 0.0f, 0.0f, 0.0f};
    hmm_vec4 v4 = { 0.0f, 0.0f, 0.0f, 1.0f};

    hmm_vec4 cells[5][4] = {
        {v0, v1, v2, v3},
        {v4, v1, v2, v3},
        {v0, v4, v2, v3},
        {v0, v1, v4, v3},
        {v0, v1, v2, v4},
    };

    hmm_vec4 edges[10][2] = {
        {v1, v0},
        {v2, v0},
        {v3, v0},
        {v4, v0},
        {v2, v1},
        {v3, v1},
        {v4, v1},
        {v3, v2},
        {v4, v2},
        {v4, v3},
    };
    // clang-format on

    auto edge_vec = [](hmm_vec4* edge) -> hmm_vec4 { return edge[1] - edge[0]; };

    hmm_vec4 hplane_p0 = {0.0f, 0.0f, 0.0f, 0.0f};
    hmm_vec4 hplane_n = {0.0f, 0.0f, 0.0f, 1.0f};

    std::vector<hmm_vec4> intersection_points;

    for (int i = 0; i < 10; i++) {
        hmm_vec4 l0 = edges[i][0];
        hmm_vec4 l = edge_vec(edges[i]);
        float d = HMM_Dot(hplane_p0 - l0, hplane_n) / HMM_Dot(l, hplane_n);
        if (d >= 0.0f && d <= 1.0f) {
            hmm_vec4 intersect = d * l + l0;
            intersection_points.push_back(intersect);
        }
    }

    printf("%lu points of intersection\n", intersection_points.size());

    std::vector<hmm_vec3> points;
    for (auto& p : intersection_points) {
        points.push_back(p.XYZ);
    }

    uint32_t vert_shader = compile_shader("data/vertex_point.glsl", GL_VERTEX_SHADER);
    if (!vert_shader) {
        return 1;
    }

    uint32_t frag_shader = compile_shader("data/fragment_point.glsl", GL_FRAGMENT_SHADER);
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
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(points[0]), points.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(points[0]), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);

#if 0
    Vertex v1, v2, v3, v4;
    // clang-format off
    v1.pos = {-1.0f, -1.0f,  1.0f};
    v2.pos = { 1.0f, -1.0f,  1.0f};
    v3.pos = { 0.0f, -1.0f, -1.0f};
    v4.pos = { 0.0f,  1.0f,  0.0f};
    // clang-format on

    Face faces[4];
    memset(faces, 0, sizeof(faces));

    faces[0].vs[0] = v1;
    faces[0].vs[1] = v3;
    faces[0].vs[2] = v2;

    faces[1].vs[0] = v1;
    faces[1].vs[1] = v2;
    faces[1].vs[2] = v4;

    faces[2].vs[0] = v3;
    faces[2].vs[1] = v1;
    faces[2].vs[2] = v4;

    faces[3].vs[0] = v2;
    faces[3].vs[1] = v3;
    faces[3].vs[2] = v4;

    for (uint32_t i = 0; i < sizeof(faces) / sizeof(*faces); i++) {
        fill_normal(faces[i]);
    }

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
    glBufferData(GL_ARRAY_BUFFER, sizeof(faces), faces, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);

    int model_location = glGetUniformLocation(shader_prog, "model");
    int mvp_location = glGetUniformLocation(shader_prog, "mvp");
    int camera_pos_location = glGetUniformLocation(shader_prog, "camera_pos");

    hmm_mat4 model = HMM_Translate(HMM_Vec3(0.0f, 0.0f, 0.0f));
#endif

    int vp_location = glGetUniformLocation(shader_prog, "vp");

    hmm_vec4 camera_pos = {0.0f, 0.0f, 8.0f, 1.0f};
    hmm_vec4 camera_target = {0.0f, 0.0f, 0.0f, 1.0f};
    hmm_vec4 camera_up = {0.0f, 1.0f, 0.0f, 1.0f};

    hmm_mat4 projection = HMM_Perspective(45.0f, (float)window_width / (float)window_height, 0.1f, 100.0f);

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
            float degrees_per_sec = 45.0f;
            camera_pos = HMM_Rotate(degrees_per_sec * (float)(step_ms / 1000), HMM_Vec3(0.0f, 1.0f, 0.0f)) * camera_pos;
            lag_ms -= step_ms;
            steps++;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        hmm_mat4 view = HMM_LookAt(camera_pos.XYZ, camera_target.XYZ, camera_up.XYZ);
        hmm_mat4 vp = projection * view;
        glUniformMatrix4fv(vp_location, 1, false, (float*)&vp);

        glDrawArrays(GL_POINTS, 0, (GLsizei)points.size());
        SDL_GL_SwapWindow(window);
    }
main_loop_end:

    return 0;
}
