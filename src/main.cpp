#include <four/app_state.hpp>
#include <four/generate.hpp>
#include <four/render.hpp>
#include <four/resource.hpp>

#include <SDL.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <loguru.hpp>

#include <stdio.h>

#ifdef __WIN32__
#    include <windows.h>
#endif

using namespace four;

int main(int argc, char** argv) {

#ifdef __WIN32__
    static bool opened_console = false;
#endif

    struct DeferFinishConsole {
        ~DeferFinishConsole() {
#ifdef __WIN32__
            if (opened_console) {
                printf("Program has finished.\n");
                getchar();
            }
#endif
        }
    } defer_finish_console;

    bool debug = false;
    bool open_console = false;

    for (s32 i = 0; i < argc; i++) {
        auto arg = argv[i];
        if (c_str_eq(arg, "-d")) {
            debug = true;
            open_console = true;
        } else if (c_str_eq(arg, "--generate")) {
            open_console = true;
        }
    }

    if (open_console) {
#ifdef __WIN32__
        CHECK_F(!opened_console);
        FreeConsole();
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        opened_console = true;
#endif
    }

    loguru::g_stderr_verbosity = 1;
    loguru::init(argc, argv);
    if (debug) {
        loguru::add_file("four.log", loguru::Append, loguru::Verbosity_MAX);
    }

    init_resource_path();

    for (s32 i = 0; i < argc; i++) {
        auto arg = argv[i];
        if (c_str_eq(arg, "--generate")) {
            CHECK_LT_F(i + 1, argc);
            const char* arg1 = argv[i + 1];

            Mesh4 mesh;
            if (c_str_eq(arg1, "5-cell")) {
                mesh = generate_5cell();

            } else if (c_str_eq(arg1, "tesseract")) {
                mesh = generate_tesseract();

            } else if (c_str_eq(arg1, "16-cell")) {
                mesh = generate_16cell();

            } else if (c_str_eq(arg1, "24-cell")) {
                mesh = generate_24cell();

            } else if (c_str_eq(arg1, "120-cell")) {
                mesh = generate_120cell();

            } else if (c_str_eq(arg1, "600-cell")) {
                mesh = generate_600cell();

            } else {
                ABORT_F("Unknown regular convex mesh4 %s", arg1);
            }

            tetrahedralize(mesh);

            auto path = std::string(arg1) + ".mesh4";
            CHECK_F(save_mesh_to_file(mesh, path.c_str()));
            return 0;
        }
    }

    SDL_Window* window = NULL;
    ImGuiIO* imgui_io = NULL;

    {
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        CHECK_EQ_F(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), 0, "%s", SDL_GetError());

        s32 error = SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1)
                    | SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
                    | SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
                    | SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
                    | SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) | SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8)
                    | SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

        if (error != 0) {
            ABORT_F("%s", SDL_GetError());
        }

        window = SDL_CreateWindow("Four", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

        CHECK_NOTNULL_F(window, "%s", SDL_GetError());

        SDL_GLContext gl_context = SDL_GL_CreateContext(window);
        CHECK_NOTNULL_F(gl_context, "%s", SDL_GetError());

        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            ABORT_F("OpenGL could not be loaded");
        }

        ImGui::CreateContext();
        imgui_io = &ImGui::GetIO();
        ImGui::StyleColorsDark();
        CHECK_F(ImGui_ImplSDL2_InitForOpenGL(window, gl_context));
        CHECK_F(ImGui_ImplOpenGL3_Init("#version 330 core"));
    }

    struct DeferSDLQuit {
        ~DeferSDLQuit() {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            SDL_Quit();
        }
    } defer_sdl_quit;

    SDL_Surface* icon = SDL_LoadBMP(get_resource_path("icon.bmp").c_str());
    CHECK_NOTNULL_F(icon);
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);

    AppState state(window, imgui_io, get_resource_path("meshes/tesseract.mesh4").c_str());
    state.debug = debug;

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
        renderer.render();
        frames++;
    }

    return 0;
}
