#pragma once

#include <four/utility.hpp>

#include <HandmadeMath.h>

#include <assert.h>
#include <stdlib.h>

namespace four {

union Vec5 {
    f64 elements[5];
    struct {
        f64 X, Y, Z, W, V;
    };

    f64& operator[](size_t index) {
        return elements[index];
    }
};

union Mat3 {
    f64 elements[3][3];
    hmm_vec3 columns[3];

    hmm_vec3& operator[](size_t index) {
        return columns[index];
    }
};

union Mat5 {
    f64 elements[5][5];
    Vec5 columns[5];

    Vec5& operator[](size_t index) {
        return columns[index];
    }
};

inline hmm_vec2 vec2(hmm_vec3 v) {
    hmm_vec2 result = {};
    result.X = v.X;
    result.Y = v.Y;
    return result;
}

inline hmm_vec3 vec3(hmm_vec4 v) {
    hmm_vec3 result = {};
    result.X = v.X;
    result.Y = v.Y;
    result.Z = v.Z;
    return result;
}

inline hmm_vec4 vec4(Vec5 v) {
    hmm_vec4 result = {};
    result.X = v.X;
    result.Y = v.Y;
    result.Z = v.Z;
    result.W = v.W;
    return result;
}

inline Vec5 vec5(f64 x, f64 y, f64 z, f64 w, f64 v) {
    Vec5 result = {};
    result.X = x;
    result.Y = y;
    result.Z = z;
    result.W = w;
    result.V = v;
    return result;
}

inline Vec5 vec5(hmm_vec4 vec, f64 v) {
    return vec5(vec.X, vec.Y, vec.Z, vec.W, v);
}

inline Mat3 mat3(hmm_vec3 column0, hmm_vec3 column1, hmm_vec3 column2) {
    Mat3 result = {.columns = {column0, column1, column2}};
    return result;
}

inline hmm_mat4 mat4(hmm_vec4 column0, hmm_vec4 column1, hmm_vec4 column2, hmm_vec4 column3) {
    hmm_mat4 result = {};

    for (s32 row = 0; row < 4; row++) {
        result.Elements[0][row] = column0[row];
    }
    for (s32 row = 0; row < 4; row++) {
        result.Elements[1][row] = column1[row];
    }
    for (s32 row = 0; row < 4; row++) {
        result.Elements[2][row] = column2[row];
    }
    for (s32 row = 0; row < 4; row++) {
        result.Elements[3][row] = column3[row];
    }

    return result;
}

inline Mat5 mat5(Vec5 column0, Vec5 column1, Vec5 column2, Vec5 column3, Vec5 column4) {
    Mat5 result = {.columns = {column0, column1, column2, column3, column4}};
    return result;
}

inline Vec5 operator*(Vec5 v, f64 x) {
    return vec5(v.X * x, v.Y * x, v.Z * x, v.W * x, v.V * x);
}

inline Vec5 operator*(f64 x, Vec5 v) {
    return v * x;
}

inline Vec5 operator*(Mat5 m, Vec5 v) {
    // Code adapted from HandmadeMath.h.

    Vec5 result;

    for (s32 r = 0; r < 5; r++) {
        f64 sum = 0;
        for (s32 c = 0; c < 5; c++) {
            sum += m.elements[c][r] * v.elements[c];
        }

        result.elements[r] = sum;
    }

    return result;
}

inline Mat5 operator*(Mat5 m1, Mat5 m2) {
    // Code adapted from HandmadeMath.h.

    Mat5 result;

    for (s32 c = 0; c < 5; c++) {
        for (s32 r = 0; r < 5; r++) {
            f64 sum = 0;
            for (s32 current_matrice = 0; current_matrice < 5; current_matrice++) {
                sum += m1.elements[current_matrice][r] * m2.elements[c][current_matrice];
            }

            result.elements[c][r] = sum;
        }
    }

    return result;
}

inline hmm_vec3 transform(hmm_mat4 m, hmm_vec3 v) {
    return vec3(m * HMM_Vec4v(v, 1));
}

inline f64 determinant(Mat3 m) {
    f64 a = m[0][0];
    f64 b = m[1][0];
    f64 c = m[2][0];
    f64 d = m[0][1];
    f64 e = m[1][1];
    f64 f = m[2][1];
    f64 g = m[0][2];
    f64 h = m[1][2];
    f64 i = m[2][2];
    return (a * e * i) + (b * f * g) + (c * d * h) - (c * e * g) - (b * d * i) - (a * f * h);
}

inline hmm_vec4 cross(hmm_vec4 u, hmm_vec4 v, hmm_vec4 w) {
    Mat3 m1 = mat3(HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.Z, v.Z, w.Z), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m2 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Z, v.Z, w.Z), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m3 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m4 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.Z, v.Z, w.Z));
    return HMM_Vec4(determinant(m1), -determinant(m2), determinant(m3), -determinant(m4));
}

inline Mat5 translate(hmm_vec4 v) {
    return mat5(vec5(1, 0, 0, 0, 0), vec5(0, 1, 0, 0, 0), vec5(0, 0, 1, 0, 0), vec5(0, 0, 0, 1, 0),
                vec5(v.X, v.Y, v.Z, v.W, 1));
}

// `angle` is in degrees
inline hmm_mat4 rotate(hmm_vec3 center, f64 angle, hmm_vec3 axis) {
    return HMM_Translate(center) * HMM_Rotate(angle, axis) * HMM_Translate(-1 * center);
}

inline Mat5 look_at(hmm_vec4 eye, hmm_vec4 target, hmm_vec4 up, hmm_vec4 over) {
    Mat5 m_t = translate(-1 * eye);
    hmm_vec4 f = HMM_Normalize(eye - target);
    hmm_vec4 l = HMM_Normalize(cross(up, over, f));
    hmm_vec4 u = HMM_Normalize(cross(over, f, l));
    hmm_vec4 o = cross(f, l, u);
    Mat5 m_r = mat5(vec5(l.X, u.X, o.X, f.X, 0), vec5(l.Y, u.Y, o.Y, f.Y, 0), vec5(l.Z, u.Z, o.Z, f.Z, 0),
                    vec5(l.W, u.W, o.W, f.W, 0), vec5(0, 0, 0, 0, 1));
    return m_r * m_t;
}

inline hmm_vec4 project_orthographic(Vec5 v, f64 near) {
    assert(near > 0.0);
    assert(v.W <= -near);
    return HMM_Vec4(v.X, v.Y, v.Z, -v.W - near);
}

inline hmm_vec4 project_perspective(Vec5 v, f64 near) {
    assert(near > 0.0);
    assert(v.W <= -near);
    f64 d = near / -v.W;
    hmm_vec4 v_4 = vec4(v);
    return HMM_Vec4(d * v.X, d * v.Y, d * v.Z, HMM_Length(v_4 - d * v_4));
}

// 4D Rotors
// =========

struct Bivec4 {
    f64 xy, xz, xw, yz, yw, zw;
};

struct Rotor4 {
    f64 s;
    Bivec4 B; // This stores (b `outer` a) for a rotor (ab)
};

inline Rotor4 rotor4(hmm_vec4 a, hmm_vec4 b) {
    a = HMM_Normalize(a);
    b = HMM_Normalize(b);

    Rotor4 result = {};
    result.s = HMM_Dot(a, b);
    result.B.xy = (b.X * a.Y) - (b.Y * a.X);
    result.B.xz = (b.X * a.Z) - (b.Z * a.X);
    result.B.xw = (b.X * a.W) - (b.W * a.X);
    result.B.yz = (b.Y * a.Z) - (b.Z * a.Y);
    result.B.yw = (b.Y * a.W) - (b.W * a.Y);
    result.B.zw = (b.Z * a.W) - (b.W * a.Z);
    return result;
}

inline hmm_vec4 rotate(Rotor4 r, hmm_vec4 v) {
    const Bivec4& B = r.B;

    // (ba)v -- vector part
    hmm_vec4 q = {};
    q.X = (r.s * v.X) + (B.xy * v.Y) + (B.xz * v.Z) + (B.xw * v.W);
    q.Y = (r.s * v.Y) - (B.xy * v.X) + (B.yz * v.Z) + (B.yw * v.W);
    q.Z = (r.s * v.Z) - (B.xz * v.X) - (B.yz * v.Y) + (B.zw * v.W);
    q.W = (r.s * v.W) - (B.xw * v.X) - (B.yw * v.Y) - (B.zw * v.Z);

    // (ba)v -- trivector part
    f64 q_xyz = (B.xy * v.Z) - (B.xz * v.Y) + (B.yz * v.X);
    f64 q_xyw = (B.xy * v.W) - (B.xw * v.Y) + (B.yw * v.X);
    f64 q_xzw = (B.xz * v.W) - (B.xw * v.Z) + (B.zw * v.X);
    f64 q_yzw = (B.yz * v.W) - (B.yw * v.Z) + (B.zw * v.Y);

    hmm_vec4 result = {};
    result.X =
            (r.s * q.X) + (q.Y * B.xy) + (q.Z * B.xz) + (q.W * B.xw) + (q_xyz * B.yz) + (q_xyw * B.yw) + (q_xzw * B.zw);
    result.Y =
            (r.s * q.Y) - (q.X * B.xy) + (q.Z * B.yz) + (q.W * B.yw) - (q_xyz * B.xz) - (q_xyw * B.xw) + (q_yzw * B.zw);
    result.Z =
            (r.s * q.Z) - (q.X * B.xz) - (q.Y * B.yz) + (q.W * B.zw) + (q_xyz * B.xy) - (q_xzw * B.xw) - (q_yzw * B.yw);
    result.W =
            (r.s * q.W) - (q.X * B.xw) - (q.Y * B.yw) - (q.Z * B.zw) + (q_xyz * B.xy) + (q_xzw * B.xz) + (q_yzw * B.yz);

    return result;
}
} // namespace four
