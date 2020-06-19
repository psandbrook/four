#pragma once

#include <HandmadeMath.h>

#include <stdlib.h>

namespace four {

struct Vec5 {
    float elements[5];

    float& operator[](size_t index) {
        return elements[index];
    }
};

union Mat3 {
    float elements[3][3];
    hmm_vec3 columns[3];

    hmm_vec3& operator[](size_t index) {
        return columns[index];
    }
};

union Mat5 {
    float elements[5][5];
    Vec5 columns[5];

    Vec5& operator[](size_t index) {
        return columns[index];
    }
};

inline Vec5 operator*(Mat5 m, Vec5 v) {
    // Code adapted from HandmadeMath.h.

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

inline Mat5 operator*(Mat5 m1, Mat5 m2) {
    // Code adapted from HandmadeMath.h.

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

inline hmm_vec3 vec3(hmm_vec4 v) {
    return {v[0], v[1], v[2]};
}

inline hmm_vec4 vec4(Vec5 v) {
    return {v[0], v[1], v[2], v[3]};
}

inline Vec5 vec5(hmm_vec4 vec, float v) {
    return {vec[0], vec[1], vec[2], vec[3], v};
}

inline Mat3 mat3(hmm_vec3 column0, hmm_vec3 column1, hmm_vec3 column2) {
    Mat3 result = {.columns = {column0, column1, column2}};
    return result;
}

inline Mat5 mat5(Vec5 column0, Vec5 column1, Vec5 column2, Vec5 column3, Vec5 column4) {
    Mat5 result = {.columns = {column0, column1, column2, column3, column4}};
    return result;
}

inline hmm_vec3 transform(hmm_vec3 v, hmm_mat4 m) {
    return vec3(m * HMM_Vec4v(v, 1));
}

inline float determinant(Mat3 m) {
    float a = m[0][0];
    float b = m[1][0];
    float c = m[2][0];
    float d = m[0][1];
    float e = m[1][1];
    float f = m[2][1];
    float g = m[0][2];
    float h = m[1][2];
    float i = m[2][2];
    return (a * e * i) + (b * f * g) + (c * d * h) - (c * e * g) - (b * d * i) - (a * f * h);
}

inline hmm_vec4 cross(hmm_vec4 u, hmm_vec4 v, hmm_vec4 w) {
    Mat3 m1 = mat3({u.Y, v.Y, w.Y}, {u.Z, v.Z, w.Z}, {u.W, v.W, w.W});
    Mat3 m2 = mat3({u.X, v.X, w.X}, {u.Z, v.Z, w.Z}, {u.W, v.W, w.W});
    Mat3 m3 = mat3({u.X, v.X, w.X}, {u.Y, v.Y, w.Y}, {u.W, v.W, w.W});
    Mat3 m4 = mat3({u.X, v.X, w.X}, {u.Y, v.Y, w.Y}, {u.Z, v.Z, w.Z});
    return {determinant(m1), -determinant(m2), determinant(m3), -determinant(m4)};
}

inline Mat5 translate(hmm_vec4 v) {
    return mat5({1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {v.X, v.Y, v.Z, v.W, 1});
}

inline hmm_mat4 rotate(hmm_vec3 center, float angle, hmm_vec3 axis) {
    return HMM_Translate(center) * HMM_Rotate(angle, axis) * HMM_Translate(-1 * center);
}

inline Mat5 look_at(hmm_vec4 eye, hmm_vec4 target, hmm_vec4 up, hmm_vec4 over) {
    Mat5 m_t = translate(-1 * eye);
    hmm_vec4 f = HMM_Normalize(eye - target);
    hmm_vec4 l = HMM_Normalize(cross(up, over, f));
    hmm_vec4 u = HMM_Normalize(cross(over, f, l));
    hmm_vec4 o = cross(f, l, u);
    Mat5 m_r = mat5({l.X, u.X, o.X, f.X, 0}, {l.Y, u.Y, o.Y, f.Y, 0}, {l.Z, u.Z, o.Z, f.Z, 0}, {l.W, u.W, o.W, f.W, 0},
                    {0, 0, 0, 0, 1});
    return m_r * m_t;
}
} // namespace four
