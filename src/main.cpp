#include <glad/glad.h>
#include <SDL.h>

#include <stdio.h>

#include <stdexcept>

// TODO: Change this so throwing an exception is not necessary
class SdlContext {
public:
    SdlContext() {
        if (SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error("SDL could not be initialised.");
        }
    }
    ~SdlContext() {
        SDL_Quit();
    }
};

int main() {
    SdlContext sdl_context;

    // FIXME: Check these function calls for errors
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    int window_width = 1280;
    int window_height = 720;

    SDL_Window* window = SDL_CreateWindow(
            "4dtest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            window_width, window_height, SDL_WINDOW_OPENGL);
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
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    while (true) {
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    return 0;
}
