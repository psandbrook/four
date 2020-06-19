#include <four/app_state.hpp>

#include <four/math.hpp>
#include <loguru.hpp>

#include <SDL.h>

namespace four {

namespace {

const f64 mouse_motion_fac = 0.002;

}

AppState new_app_state(const char* mesh_path) {
    AppState state;
    state.mesh = load_mesh_from_file(mesh_path);
    return state;
}

bool handle_events(AppState& s) {

    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        switch (event.type) {
        case SDL_QUIT:
            return true;

        case SDL_WINDOWEVENT: {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                s.window_size_changed = true;
            }
        } break;

        case SDL_KEYDOWN: {
            if (!event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return true;
                }
            }
        } break;

        case SDL_MOUSEMOTION: {
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                if (SDL_GetModState() & KMOD_SHIFT) {

                    hmm_vec3 front = s.camera_target - s.camera_pos;
                    f64 distance_fac = 0.25 * HMM_Length(front);

                    hmm_vec3 f = HMM_Normalize(front);
                    hmm_vec3 left = HMM_Normalize(HMM_Cross(HMM_Vec3(0, 1, 0), f));
                    hmm_vec3 up = HMM_Cross(f, left);

                    f64 x_move = mouse_motion_fac * distance_fac * event.motion.xrel;
                    f64 y_move = mouse_motion_fac * distance_fac * event.motion.yrel;

                    hmm_mat4 y_trans = HMM_Translate(y_move * up);
                    hmm_mat4 x_trans = HMM_Translate(x_move * left);
                    hmm_mat4 translation = y_trans * x_trans;

                    s.camera_pos = transform(translation, s.camera_pos);
                    s.camera_target = transform(translation, s.camera_target);

                } else {
                    f64 x_angle = mouse_motion_fac * event.motion.xrel;
                    Rotor3 x_rotor = rotor3(x_angle, outer(HMM_Vec3(0, 0, -1), HMM_Vec3(1, 0, 0)));

                    f64 y_angle = mouse_motion_fac * event.motion.yrel;
                    Rotor3 y_rotor = rotor3(y_angle, outer(HMM_Vec3(0, 1, 0), s.camera_target - s.camera_pos));

                    hmm_mat4 rotation = to_mat4(y_rotor * x_rotor);
                    hmm_mat4 m = HMM_Translate(s.camera_target) * rotation * HMM_Translate(-1 * s.camera_target);

                    hmm_vec3 new_camera_pos = transform(m, s.camera_pos);
                    hmm_vec3 front = HMM_Normalize(s.camera_target - new_camera_pos);
                    if (!float_eq(std::abs(front.Y), 1.0, 0.001)) {
                        s.camera_pos = new_camera_pos;
                    }
                }
            }
        } break;

        case SDL_MOUSEWHEEL: {
            f64 y = event.wheel.y;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                y *= -1;
            }

            hmm_vec3 front = s.camera_target - s.camera_pos;
            f64 distance_fac = 0.1 * HMM_Length(front);
            hmm_vec3 f = HMM_Normalize(front);
            hmm_mat4 translation = HMM_Translate(y * distance_fac * f);
            s.camera_pos = transform(translation, s.camera_pos);

        } break;
        }
    }

    return false;
}

void step_state(AppState& s, const f64 ms) {

    Rotor4 r = rotor4({1, 0, 0, 0}, {1, 0, 0, 0.002});

    for (size_t i = 0; i < s.mesh.vertices.size(); i++) {
        s.mesh.vertices[i] = rotate(r, s.mesh.vertices[i]);
    }
}
} // namespace four
