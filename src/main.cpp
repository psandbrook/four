#include <four/app_state.hpp>
#include <four/render.hpp>

#include <SDL.h>
#include <glad/glad.h>
#include <loguru.hpp>

using namespace four;

namespace {

struct SdlGuard {
    SDL_Window* window;

    SdlGuard() {
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
#if 0
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#endif

        window = SDL_CreateWindow("four", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);

        CHECK_NOTNULL_F(window, "%s", SDL_GetError());

        CHECK_NOTNULL_F(SDL_GL_CreateContext(window), "%s", SDL_GetError());

        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            ABORT_F("OpenGL could not be loaded");
        }
    }

    ~SdlGuard() {
        SDL_Quit();
    }
};
} // namespace

int main(int argc, char** argv) {
    loguru::init(argc, argv);

    SdlGuard sdl_guard;
    AppState state = new_app_state("data/tesseract.mesh4");
    Renderer renderer(sdl_guard.window, &state);

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
        if (handle_events(state)) {
            break;
        }

        // Run simulation
        s32 steps = 0;
        while (lag_ms >= step_ms && steps < steps_per_sec) {
            step_state(state, step_ms);
            lag_ms -= step_ms;
            steps++;
        }

        // Render
        renderer.render();
        frames++;
    }

    return 0;
}
