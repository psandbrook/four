#include <four/app_state.hpp>
#include <four/render.hpp>

#include <SDL.h>
#include <glad/glad.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <loguru.hpp>

#include <chrono>
#include <thread>

using namespace four;

namespace {

struct WindowGuard {
    SDL_Window* window;
    ImGuiIO* imgui_io;

    WindowGuard() {
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        CHECK_EQ_F(SDL_Init(SDL_INIT_VIDEO), 0, "%s", SDL_GetError());

        if (SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1) != 0
            || SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) != 0
            || SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) != 0
            || SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0
            || SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1) != 0) {

            ABORT_F("%s", SDL_GetError());
        }

        // Enable multisampling
#ifndef FOUR_DEBUG
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);
#endif

        window = SDL_CreateWindow("four", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

        CHECK_NOTNULL_F(window, "%s", SDL_GetError());

        SDL_GLContext gl_context = SDL_GL_CreateContext(window);
        CHECK_NOTNULL_F(gl_context, "%s", SDL_GetError());

        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            ABORT_F("OpenGL could not be loaded");
        }

        ImGui::CreateContext();
        imgui_io = &ImGui::GetIO();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 450");
    }

    ~WindowGuard() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_Quit();
    }
};
} // namespace

int main(int argc, char** argv) {
    loguru::init(argc, argv);

    WindowGuard window_guard;
    auto& window = window_guard.window;
    auto& imgui_io = window_guard.imgui_io;

    AppState state(window, imgui_io, "data/tesseract.mesh4");
    Renderer renderer(window, &state);

    const f64 count_per_ms = (f64)SDL_GetPerformanceFrequency() / 1000.0;
    const s32 steps_per_sec = 60;
    const f64 step_ms = 1000.0 / steps_per_sec;

    f64 lag_ms = 0.0;
    u64 last_count = SDL_GetPerformanceCounter();
    f64 second_acc = 0.0;
    s32 frames = 0;

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
        if (state.process_events_and_imgui()) {
            break;
        }

        // Run simulation
        s32 steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {
            state.step(step_ms);
            lag_ms -= step_ms;
            steps++;
        }

        // Render
        if (steps == 0) {
            f64 sleep_ms = step_ms - lag_ms - 1.0;
            if (sleep_ms > 0.0) {
                s64 sleep_ns = (s64)(sleep_ms * 1000000.0);
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
            }
        } else {
            renderer.render();
            frames++;
        }
    }

    return 0;
}
