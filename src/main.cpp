#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

#include <SDL.h>
#include <glad/glad.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace {

class SdlGuard {
public:
    ~SdlGuard() {
        SDL_Quit();
    }
};

struct Mat5 {
    float elements[5][5];
};

struct Vec5 {
    float elements[5];

    float& operator[](size_t index) {
        return elements[index];
    }

    float operator[](size_t index) const {
        return elements[index];
    }
};

hmm_vec4 vec4(const Vec5& v) {
    return HMM_Vec4(v[0], v[1], v[2], v[3]);
}

Vec5 vec5(hmm_vec4 vec, float v) {
    return {vec[0], vec[1], vec[2], vec[3], v};
}

Vec5 operator*(Mat5 m, const Vec5& v) {
    Vec5 result;

    for (int r = 0; r < 5; r++) {
        float sum = 0;
        for (int c = 0; c < 5; c++) {
            sum += m.elements[c][r] * v.elements[c];
        }

        result.elements[r] = sum;
    }

    return result;
}

Mat5 operator*(Mat5 m1, const Mat5& m2) {
    Mat5 result;

    for (int c = 0; c < 5; c++) {
        for (int r = 0; r < 5; r++) {
            float sum = 0;
            for (int current_matrice = 0; current_matrice < 5; current_matrice++) {
                sum += m1.elements[current_matrice][r] * m2.elements[c][current_matrice];
            }

            result.elements[c][r] = sum;
        }
    }

    return result;
}

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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    struct Tetrahedron {
        hmm_vec4 vertices[4];
    };

    // clang-format off
    hmm_vec4 vertices[] = {
        {1, 1, 1, -1/sqrtf(5)},
        {1, -1, -1, -1/sqrtf(5)},
        {-1, 1, -1, -1/sqrt(5)},
        {-1, -1, 1, -1/sqrtf(5)},
        {0, 0, 0, sqrtf(5) - 1/sqrtf(5)},
    };

    Tetrahedron tets[] = {
        {{vertices[0], vertices[1], vertices[2], vertices[3]}},
        {{vertices[4], vertices[1], vertices[2], vertices[3]}},
        {{vertices[0], vertices[4], vertices[2], vertices[3]}},
        {{vertices[0], vertices[1], vertices[4], vertices[3]}},
        {{vertices[0], vertices[1], vertices[2], vertices[4]}},
    };
    // clang-format on

    // TODO: For arbitrary hyperplanes, every point in the world must be
    // transformed so that the hyperplane has p_0 = (x, y, z, 0) and n = (0, 0,
    // 0, 1).
    hmm_vec4 hplane_p0 = {0.0f, 0.0f, 0.0f, 0.0f};
    hmm_vec4 hplane_n = {0.0f, 0.0f, 0.0f, 1.0f};

    struct Triangle {
        hmm_vec3 vertices[3];
    };

    std::vector<Triangle> cross_tris;

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
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(hmm_vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    int vp_location = glGetUniformLocation(shader_prog, "vp");

    hmm_vec3 camera_pos = {0.0f, 0.0f, 8.0f};
    hmm_vec3 camera_target = {0.0f, 0.0f, 0.0f};
    hmm_vec3 camera_up = {0.0f, 1.0f, 0.0f};

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
            camera_pos = (HMM_Rotate(degrees_per_sec * (float)(step_ms / 1000.0), HMM_Vec3(0.0f, 1.0f, 0.0f))
                          * HMM_Vec4v(camera_pos, 1.0f))
                                 .XYZ;

            float rad = 0.01f;
            Mat5 rotation_mat = {{
                    {cosf(rad), 0, 0, -sinf(rad), 0},
                    {0, 1, 0, 0, 0},
                    {0, 0, 1, 0, 0},
                    {sinf(rad), 0, 0, cosf(rad), 0},
                    {0, 0, 0, 0, 1},
            }};

            for (auto& tet : tets) {
                for (int i = 0; i < 4; i++) {
                    tet.vertices[i] = vec4(rotation_mat * vec5(tet.vertices[i], 1.0f));
                }
            }

            lag_ms -= step_ms;
            steps++;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cross_tris.clear();

        for (auto& tet : tets) {
            // clang-format off
            std::pair<hmm_vec4, hmm_vec4> edges[6] = {
                {tet.vertices[0], tet.vertices[1]},
                {tet.vertices[0], tet.vertices[2]},
                {tet.vertices[0], tet.vertices[3]},
                {tet.vertices[1], tet.vertices[2]},
                {tet.vertices[1], tet.vertices[3]},
                {tet.vertices[2], tet.vertices[3]},
            };
            // clang-format on

            std::vector<hmm_vec3> isect_points;

            for (auto& e : edges) {
                hmm_vec4& l0 = e.first;
                hmm_vec4& l1 = e.second;
                hmm_vec4 l = l1 - l0;
                float d = -l0.W / l.W;
                if (d >= 0.0f && d <= 1.0f) {
                    hmm_vec4 intersect = d * l + l0;
                    assert(intersect.W <= std::numeric_limits<float>::epsilon());
                    isect_points.push_back(intersect.XYZ);
                }
            }

            if (isect_points.size() == 3) {
                Triangle tri;
                tri.vertices[0] = isect_points[0];
                tri.vertices[1] = isect_points[1];
                tri.vertices[2] = isect_points[2];

                cross_tris.push_back(tri);

            } else if (isect_points.size() == 4) {
                Triangle tri1, tri2;
                float len1 = HMM_LengthSquared(isect_points[0] - isect_points[2]);
                float len2 = HMM_LengthSquared(isect_points[1] - isect_points[3]);

                if (len1 < len2) {
                    tri1.vertices[0] = isect_points[0];
                    tri1.vertices[1] = isect_points[1];
                    tri1.vertices[2] = isect_points[2];

                    tri2.vertices[0] = isect_points[0];
                    tri2.vertices[1] = isect_points[2];
                    tri2.vertices[2] = isect_points[3];
                } else {
                    tri1.vertices[0] = isect_points[0];
                    tri1.vertices[1] = isect_points[1];
                    tri1.vertices[2] = isect_points[3];

                    tri2.vertices[0] = isect_points[3];
                    tri2.vertices[1] = isect_points[1];
                    tri2.vertices[2] = isect_points[2];
                }

                cross_tris.push_back(tri1);
                cross_tris.push_back(tri2);
            }
        }

        auto print_vec3 = [](hmm_vec3& v) { printf("x: %.2f, y: %.2f, z: %.2f\n", v[0], v[1], v[2]); };

        printf("start\n");
        for (auto& tri : cross_tris) {
            printf("start tri\n");
            print_vec3(tri.vertices[0]);
            print_vec3(tri.vertices[1]);
            print_vec3(tri.vertices[2]);
            printf("end tri\n");
        }
        printf("end\n");

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, cross_tris.size() * sizeof(cross_tris[0]), cross_tris.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        hmm_mat4 view = HMM_LookAt(camera_pos, camera_target, camera_up);
        hmm_mat4 vp = projection * view;
        glUniformMatrix4fv(vp_location, 1, false, (float*)&vp);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(cross_tris.size() * 3));
        glBindVertexArray(0);

        SDL_GL_SwapWindow(window);
    }
main_loop_end:

    return 0;
}
