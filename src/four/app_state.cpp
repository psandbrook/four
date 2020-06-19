#include <four/app_state.hpp>
#include <four/math.hpp>

#include <SDL.h>

namespace four {

AppState new_app_state(const char* mesh_path) {
    AppState state;
    state.mesh = load_mesh_from_file(mesh_path);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    return state;
}

bool handle_events(AppState& s) {

    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        switch (event.type) {
        case SDL_QUIT:
            return true;

        case SDL_KEYDOWN: {
            if (!event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return true;

                case SDLK_r: {
                    s.camera_move_up = true;
                } break;

                case SDLK_f: {
                    s.camera_move_down = true;
                } break;

                case SDLK_w: {
                    s.camera_move_forward = true;
                } break;

                case SDLK_s: {
                    s.camera_move_backward = true;
                } break;

                case SDLK_a: {
                    s.camera_move_left = true;
                } break;

                case SDLK_d: {
                    s.camera_move_right = true;
                } break;
                }
            }
        } break;

        case SDL_KEYUP: {
            if (!event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_r: {
                    s.camera_move_up = false;
                } break;

                case SDLK_f: {
                    s.camera_move_down = false;
                } break;

                case SDLK_w: {
                    s.camera_move_forward = false;
                } break;

                case SDLK_s: {
                    s.camera_move_backward = false;
                } break;

                case SDLK_a: {
                    s.camera_move_left = false;
                } break;

                case SDLK_d: {
                    s.camera_move_right = false;
                } break;
                }
            }
        } break;

        case SDL_MOUSEMOTION: {
            // FIXME: Use quaternions?
            f64 x_angle = 0.2 * (f64)event.motion.xrel;
            s.camera_pos = transform(rotate(s.camera_target, x_angle, s.camera_up), s.camera_pos);

            f64 y_angle = 0.2 * (f64)-event.motion.yrel;
            hmm_vec3 left = HMM_Cross(s.camera_up, s.camera_target - s.camera_pos);
            hmm_vec3 new_camera_pos = transform(rotate(s.camera_target, y_angle, left), s.camera_pos);
            hmm_vec3 front = HMM_Normalize(s.camera_target - new_camera_pos);
            if (!float_eq(std::abs(front.Y), 1.0, 0.001)) {
                s.camera_pos = new_camera_pos;
            }
        } break;
        }
    }

    return false;
}

void step_state(AppState& s, const f64 ms) {

    f64 camera_move_units = s.camera_move_units_per_sec * (ms / 1000.0);

    if (s.camera_move_up) {
        hmm_mat4 m_t = HMM_Translate(camera_move_units * s.camera_up);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    if (s.camera_move_down) {
        hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * s.camera_up);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    if (s.camera_move_forward) {
        hmm_vec3 front = s.camera_target - s.camera_pos;
        hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
        hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    if (s.camera_move_backward) {
        hmm_vec3 front = s.camera_target - s.camera_pos;
        hmm_vec3 d = HMM_NormalizeVec3({front.X, 0, front.Z});
        hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    if (s.camera_move_left) {
        hmm_vec3 left = HMM_Cross(s.camera_up, s.camera_target - s.camera_pos);
        hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
        hmm_mat4 m_t = HMM_Translate(camera_move_units * d);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    if (s.camera_move_right) {
        hmm_vec3 left = HMM_Cross(s.camera_up, s.camera_target - s.camera_pos);
        hmm_vec3 d = HMM_NormalizeVec3({left.X, 0, left.Z});
        hmm_mat4 m_t = HMM_Translate(-1 * camera_move_units * d);
        s.camera_pos = transform(m_t, s.camera_pos);
        s.camera_target = transform(m_t, s.camera_target);
    }

    Rotor4 r = rotor4({1, 0, 0, 0}, {1, 0, 0, 0.002});

    for (size_t i = 0; i < s.mesh.vertices.size(); i++) {
        s.mesh.vertices[i] = rotate(r, s.mesh.vertices[i]);
    }
}
} // namespace four
