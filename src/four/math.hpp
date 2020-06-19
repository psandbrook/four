#pragma once

#include <four/utility.hpp>

#include <HandmadeMath.h>
#include <loguru.hpp>

#include <stdlib.h>

namespace four {

inline f64 sq(f64 x) {
    return x * x;
}

union Vec5 {
    struct {
        f64 X, Y, Z, W, V;
    };
    f64 elements[5];

    f64& operator[](size_t index) {
        return elements[index];
    }

    f64 operator[](size_t index) const {
        return elements[index];
    }
};

union Mat3 {
    hmm_vec3 columns[3];
    f64 elements[3][3];

    hmm_vec3& operator[](size_t index) {
        return columns[index];
    }

    const hmm_vec3& operator[](size_t index) const {
        return columns[index];
    }
};

union Mat5 {
    Vec5 columns[5];
    f64 elements[5][5];

    Vec5& operator[](size_t index) {
        return columns[index];
    }

    const Vec5& operator[](size_t index) const {
        return columns[index];
    }
};

inline hmm_vec2 vec2(const hmm_vec3& v) {
    hmm_vec2 result = {};
    result.X = v.X;
    result.Y = v.Y;
    return result;
}

inline hmm_vec3 vec3(const hmm_vec4& v) {
    hmm_vec3 result = {};
    result.X = v.X;
    result.Y = v.Y;
    result.Z = v.Z;
    return result;
}

inline hmm_vec4 vec4(f64 x, f64 y, f64 z, f64 w) {
    hmm_vec4 result = {};
    result.X = x;
    result.Y = y;
    result.Z = z;
    result.W = w;
    return result;
}

inline hmm_vec4 vec4(const hmm_vec3& vec, f64 v) {
    return vec4(vec.X, vec.Y, vec.Z, v);
}

inline hmm_vec4 vec4(const Vec5& v) {
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

inline Vec5 vec5(const hmm_vec4& vec, f64 v) {
    return vec5(vec.X, vec.Y, vec.Z, vec.W, v);
}

inline Mat3 mat3(const hmm_vec3& column0, const hmm_vec3& column1, const hmm_vec3& column2) {
    Mat3 result = {.columns = {column0, column1, column2}};
    return result;
}

inline hmm_mat4 mat4(const hmm_vec4& column0, const hmm_vec4& column1, const hmm_vec4& column2,
                     const hmm_vec4& column3) {
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

inline Mat5 mat5(const Vec5& column0, const Vec5& column1, const Vec5& column2, const Vec5& column3,
                 const Vec5& column4) {
    Mat5 result = {.columns = {column0, column1, column2, column3, column4}};
    return result;
}

inline Vec5 operator*(const Vec5& v, f64 x) {
    return vec5(v.X * x, v.Y * x, v.Z * x, v.W * x, v.V * x);
}

inline Vec5 operator*(f64 x, const Vec5& v) {
    return v * x;
}

inline Vec5 operator*(const Mat5& m, const Vec5& v) {
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

inline Mat5 operator*(const Mat5& m1, const Mat5& m2) {
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

inline hmm_vec3 transform(const hmm_mat4& m, const hmm_vec3& v) {
    return vec3(m * HMM_Vec4v(v, 1));
}

inline hmm_mat4 look_at_inverse(const hmm_vec3& eye, const hmm_vec3& target, const hmm_vec3& up) {
    hmm_mat4 m_t = HMM_Translate(eye);
    hmm_vec3 f = HMM_Normalize(eye - target);
    hmm_vec3 l = HMM_Normalize(HMM_Cross(up, f));
    hmm_vec3 u = HMM_Cross(f, l);
    hmm_mat4 m_r = mat4(vec4(l, 0), vec4(u, 0), vec4(f, 0), vec4(0, 0, 0, 1));
    return m_t * m_r;
}

inline hmm_vec4 transform(const Mat5& m, const hmm_vec4& v) {
    return vec4(m * vec5(v, 1));
}

inline f64 determinant(const Mat3& m) {
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

inline hmm_vec4 cross(const hmm_vec4& u, const hmm_vec4& v, const hmm_vec4& w) {
    Mat3 m1 = mat3(HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.Z, v.Z, w.Z), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m2 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Z, v.Z, w.Z), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m3 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.W, v.W, w.W));
    Mat3 m4 = mat3(HMM_Vec3(u.X, v.X, w.X), HMM_Vec3(u.Y, v.Y, w.Y), HMM_Vec3(u.Z, v.Z, w.Z));
    return vec4(determinant(m1), -determinant(m2), determinant(m3), -determinant(m4));
}

inline Mat5 translate(const hmm_vec4& v) {
    return mat5(vec5(1, 0, 0, 0, 0), vec5(0, 1, 0, 0, 0), vec5(0, 0, 1, 0, 0), vec5(0, 0, 0, 1, 0),
                vec5(v.X, v.Y, v.Z, v.W, 1));
}

inline Mat5 scale(const hmm_vec4& v) {
    return mat5(vec5(v.X, 0, 0, 0, 0), vec5(0, v.Y, 0, 0, 0), vec5(0, 0, v.Z, 0, 0), vec5(0, 0, 0, v.W, 0),
                vec5(0, 0, 0, 0, 1));
}

inline Mat5 look_at(const hmm_vec4& eye, const hmm_vec4& target, const hmm_vec4& up, const hmm_vec4& over) {
    Mat5 m_t = translate(-1 * eye);
    hmm_vec4 f = HMM_Normalize(eye - target);
    hmm_vec4 l = HMM_Normalize(cross(up, over, f));
    hmm_vec4 u = HMM_Normalize(cross(over, f, l));
    hmm_vec4 o = cross(f, l, u);
    Mat5 m_r = mat5(vec5(l.X, u.X, o.X, f.X, 0), vec5(l.Y, u.Y, o.Y, f.Y, 0), vec5(l.Z, u.Z, o.Z, f.Z, 0),
                    vec5(l.W, u.W, o.W, f.W, 0), vec5(0, 0, 0, 0, 1));
    return m_r * m_t;
}

inline Mat5 look_at_inverse(const hmm_vec4& eye, const hmm_vec4& target, const hmm_vec4& up, const hmm_vec4& over) {
    Mat5 m_t = translate(eye);
    hmm_vec4 f = HMM_Normalize(eye - target);
    hmm_vec4 l = HMM_Normalize(cross(up, over, f));
    hmm_vec4 u = HMM_Normalize(cross(over, f, l));
    hmm_vec4 o = cross(f, l, u);
    Mat5 m_r = mat5(vec5(l, 0), vec5(u, 0), vec5(o, 0), vec5(f, 0), vec5(0, 0, 0, 0, 1));
    return m_t * m_r;
}

inline hmm_vec4 project_orthographic(const Vec5& v, f64 near) {
    DCHECK_GT_F(near, 0.0);
    DCHECK_LE_F(v.W, -near);
    return vec4(v.X, v.Y, v.Z, std::abs(v.W - (-near)));
}

inline hmm_vec4 project_perspective(const Vec5& v, f64 near) {
    DCHECK_GT_F(near, 0.0);
    DCHECK_LE_F(v.W, -near);
    f64 d = near / -v.W;
    hmm_vec4 intersect = d * vec4(v);
    return vec4(intersect.X, intersect.Y, intersect.Z, HMM_Length(intersect - vec4(v)));
}

// 3D Rotors
// =========

// Code adapted from http://marctenbosch.com/quaternions/code.htm

struct Bivec3 {
    f64 xy, xz, yz;
};

struct Rotor3 {
    f64 s;
    Bivec3 B; // This stores (b `outer` a) for a rotor (ab)
};

// Outer product
inline Bivec3 outer(const hmm_vec3& a, const hmm_vec3& b) {
    Bivec3 result = {};
    result.xy = (a.X * b.Y) - (a.Y * b.X);
    result.xz = (a.X * b.Z) - (a.Z * b.X);
    result.yz = (a.Y * b.Z) - (a.Z * b.Y);
    return result;
}

inline Bivec3 normalize(const Bivec3& B) {
    Bivec3 result = {};
    f64 length = sqrt(sq(B.xy) + sq(B.xz) + sq(B.yz));
    result.xy = B.xy / length;
    result.xz = B.xz / length;
    result.yz = B.yz / length;
    return result;
}

inline Rotor3 normalize(const Rotor3& r) {
    const auto& B = r.B;
    Rotor3 result = {};
    f64 length = sqrt(sq(r.s) + sq(B.xy) + sq(B.xz) + sq(B.yz));
    result.s = r.s / length;
    result.B.xy = B.xy / length;
    result.B.xz = B.xz / length;
    result.B.yz = B.yz / length;
    return result;
}

inline Rotor3 rotor3(const hmm_vec3& a, const hmm_vec3& b) {
    Rotor3 result = {};
    result.s = HMM_Dot(a, b);

    Bivec3 minus_B = outer(b, a);
    result.B.xy = minus_B.xy;
    result.B.xz = minus_B.xz;
    result.B.yz = minus_B.yz;

    result = normalize(result);
    return result;
}

// Construct the rotor that rotates `angle` radians in the given plane.
inline Rotor3 rotor3(f64 angle, const Bivec3& plane) {
    Bivec3 nplane = normalize(plane);

    Rotor3 result = {};
    result.s = cos(angle / 2.0);

    f64 sin_a = sin(angle / 2.0);
    result.B.xy = -sin_a * nplane.xy;
    result.B.xz = -sin_a * nplane.xz;
    result.B.yz = -sin_a * nplane.yz;

    return result;
}

inline Rotor3 operator*(const Rotor3& lhs, const Rotor3& rhs) {
    Rotor3 result;
    result.s = (lhs.s * rhs.s) - (lhs.B.xy * rhs.B.xy) - (lhs.B.xz * rhs.B.xz) - (lhs.B.yz * rhs.B.yz);
    result.B.xy = (lhs.B.xy * rhs.s) + (lhs.s * rhs.B.xy) + (lhs.B.yz * rhs.B.xz) - (lhs.B.xz * rhs.B.yz);
    result.B.xz = (lhs.B.xz * rhs.s) + (lhs.s * rhs.B.xz) - (lhs.B.yz * rhs.B.xy) + (lhs.B.xy * rhs.B.yz);
    result.B.yz = (lhs.B.yz * rhs.s) + (lhs.s * rhs.B.yz) + (lhs.B.xz * rhs.B.xy) - (lhs.B.xy * rhs.B.xz);
    return result;
}

inline hmm_vec3 rotate(const Rotor3& r, const hmm_vec3& v) {
    const auto& B = r.B;

    // (ba)v -- vector part
    hmm_vec3 q = {};
    q.X = (r.s * v.X) + (v.Y * B.xy) + (v.Z * B.xz);
    q.Y = (r.s * v.Y) - (v.X * B.xy) + (v.Z * B.yz);
    q.Z = (r.s * v.Z) - (v.X * B.xz) - (v.Y * B.yz);

    // (ba)v -- trivector part
    f64 q_xyz = (-v.X * B.yz) + (v.Y * B.xz) - (v.Z * B.xy);

    hmm_vec3 result = {};
    result.X = (r.s * q.X) + (q.Y * B.xy) + (q.Z * B.xz) - (q_xyz * B.yz);
    result.Y = (r.s * q.Y) - (q.X * B.xy) + (q_xyz * B.xz) + (q.Z * B.yz);
    result.Z = (r.s * q.Z) - (q_xyz * B.xy) - (q.X * B.xz) - (q.Y * B.yz);

    return result;
}

// Equivalent to conjugate for quaternions
inline Rotor3 reverse(const Rotor3& r) {
    Rotor3 result = {};
    result.s = r.s;
    result.B.xy = -r.B.xy;
    result.B.xz = -r.B.xz;
    result.B.yz = -r.B.yz;
    return result;
}

// Rotate a rotor `a` by a rotor `r`
inline Rotor3 rotate(const Rotor3& r, const Rotor3& a) {
    return r * a * reverse(r);
}

inline hmm_mat4 to_mat4(const Rotor3& r) {
    hmm_vec3 v_x = rotate(r, HMM_Vec3(1, 0, 0));
    hmm_vec3 v_y = rotate(r, HMM_Vec3(0, 1, 0));
    hmm_vec3 v_z = rotate(r, HMM_Vec3(0, 0, 1));
    return mat4(HMM_Vec4v(v_x, 0), HMM_Vec4v(v_y, 0), HMM_Vec4v(v_z, 0), vec4(0, 0, 0, 1));
}

// =========

// 4D Rotors
// =========

union Bivec4 {
    struct {
        f64 xy, xz, xw, yz, yw, zw;
    };
    f64 elements[6];

    f64& operator[](size_t index) {
        return elements[index];
    }

    f64 operator[](size_t index) const {
        return elements[index];
    }
};

struct Rotor4 {
    f64 s;
    Bivec4 B; // This stores (b `outer` a) for a rotor (ab)
    f64 xyzw; // 4-vector part
};

// Outer product
inline Bivec4 outer(const hmm_vec4& a, const hmm_vec4& b) {
    Bivec4 result = {};
    result.xy = (a.X * b.Y) - (a.Y * b.X);
    result.xz = (a.X * b.Z) - (a.Z * b.X);
    result.xw = (a.X * b.W) - (a.W * b.X);
    result.yz = (a.Y * b.Z) - (a.Z * b.Y);
    result.yw = (a.Y * b.W) - (a.W * b.Y);
    result.zw = (a.Z * b.W) - (a.W * b.Z);
    return result;
}

inline Bivec4 normalize(const Bivec4& B) {
    Bivec4 result = {};
    f64 length = sqrt(sq(B.xy) + sq(B.xz) + sq(B.xw) + sq(B.yz) + sq(B.yw) + sq(B.zw));
    result.xy = B.xy / length;
    result.xz = B.xz / length;
    result.xw = B.xw / length;
    result.yz = B.yz / length;
    result.yw = B.yw / length;
    result.zw = B.zw / length;
    return result;
}

inline Rotor4 normalize(const Rotor4& r) {
    const auto& B = r.B;
    Rotor4 result = {};
    f64 length = sqrt(sq(r.s) + sq(B.xy) + sq(B.xz) + sq(B.xw) + sq(B.yz) + sq(B.yw) + sq(B.zw) + sq(r.xyzw));
    result.s = r.s / length;
    result.B.xy = B.xy / length;
    result.B.xz = B.xz / length;
    result.B.xw = B.xw / length;
    result.B.yz = B.yz / length;
    result.B.yw = B.yw / length;
    result.B.zw = B.zw / length;
    result.xyzw = r.xyzw / length;
    return result;
}

inline Rotor4 rotor4() {
    Rotor4 result = {};
    result.s = 1;
    return result;
}

inline Rotor4 rotor4(const hmm_vec4& a, const hmm_vec4& b) {
    Rotor4 result = {};
    result.s = HMM_Dot(a, b);
    result.B.xy = (b.X * a.Y) - (b.Y * a.X);
    result.B.xz = (b.X * a.Z) - (b.Z * a.X);
    result.B.xw = (b.X * a.W) - (b.W * a.X);
    result.B.yz = (b.Y * a.Z) - (b.Z * a.Y);
    result.B.yw = (b.Y * a.W) - (b.W * a.Y);
    result.B.zw = (b.Z * a.W) - (b.W * a.Z);
    result.xyzw = 0;

    result = normalize(result);
    return result;
}

// Construct the rotor that rotates `angle` radians in the given plane.
inline Rotor4 rotor4(f64 angle, const Bivec4& plane) {
    Bivec4 nplane = normalize(plane);

    Rotor4 result = {};
    result.s = cos(angle / 2.0);

    f64 sin_a = sin(angle / 2.0);
    result.B.xy = -sin_a * nplane.xy;
    result.B.xz = -sin_a * nplane.xz;
    result.B.xw = -sin_a * nplane.xw;
    result.B.yz = -sin_a * nplane.yz;
    result.B.yw = -sin_a * nplane.yw;
    result.B.zw = -sin_a * nplane.zw;
    result.xyzw = 0;

    return result;
}

inline Rotor4 operator*(const Rotor4& lhs, const Rotor4& rhs) {
    const f64 s_D = lhs.s;
    const f64 s_B = rhs.s;
    const auto& D = lhs.B;
    const auto& B = rhs.B;

    Rotor4 result = {};

    result.s =
            (s_D * s_B) - (D.xy * B.xy) - (D.xz * B.xz) - (D.xw * B.xw) - (D.yz * B.yz) - (D.yw * B.yw) - (D.zw * B.zw);

    result.B.xy = (s_D * B.xy) + (s_B * D.xy) - (D.xw * B.yw) + (D.yw * B.xw) + (D.yz * B.xz) - (D.xz * B.yz)
                  - (D.zw * rhs.xyzw) - (lhs.xyzw * B.zw);

    result.B.xz = (s_D * B.xz) + (s_B * D.xz) + (D.xy * B.yz) - (D.yz * B.xy) - (D.xw * B.zw) + (D.zw * B.xw)
                  + (D.yw * rhs.xyzw) + (lhs.xyzw * B.yw);

    result.B.xw = (s_D * B.xw) + (s_B * D.xw) + (D.xy * B.yw) - (D.yw * B.xy) + (D.xz * B.zw) - (D.zw * B.xz)
                  - (D.yz * rhs.xyzw) - (lhs.xyzw * B.yz);

    result.B.yz = (s_D * B.yz) + (s_B * D.yz) - (D.xy * B.xz) + (D.xz * B.xy) - (D.yw * B.zw) + (D.zw * B.yw)
                  - (D.xw * rhs.xyzw) - (lhs.xyzw * B.xw);

    result.B.yw = (s_D * B.yw) + (s_B * D.yw) - (D.xy * B.xw) + (D.xw * B.xy) + (D.yz * B.zw) - (D.zw * B.yz)
                  + (D.xz * rhs.xyzw) + (lhs.xyzw * B.xz);

    result.B.zw = (s_D * B.zw) + (s_B * D.zw) - (D.xz * B.xw) + (D.xw * B.xz) - (D.yz * B.yw) + (D.yw * B.yz)
                  - (D.xy * rhs.xyzw) + (lhs.xyzw * B.xy);

    result.xyzw = (D.xy * B.zw) - (D.xz * B.yw) + (D.xw * B.yz) + (D.yz * B.xw) - (D.yw * B.xz) + (D.zw * B.xy)
                  + (s_D * rhs.xyzw) + (s_B * lhs.xyzw);

    return result;
}

inline hmm_vec4 rotate(const Rotor4& r, const hmm_vec4& v) {
    const auto& B = r.B;

    // (ba)v -- vector part
    hmm_vec4 q = {};
    q.X = (r.s * v.X) + (B.xy * v.Y) + (B.xz * v.Z) + (B.xw * v.W);
    q.Y = (r.s * v.Y) - (B.xy * v.X) + (B.yz * v.Z) + (B.yw * v.W);
    q.Z = (r.s * v.Z) - (B.xz * v.X) - (B.yz * v.Y) + (B.zw * v.W);
    q.W = (r.s * v.W) - (B.xw * v.X) - (B.yw * v.Y) - (B.zw * v.Z);

    // (ba)v -- trivector part
    f64 q_xyz = (B.xy * v.Z) - (B.xz * v.Y) + (B.yz * v.X) + (r.xyzw * v.W);
    f64 q_xyw = (B.xy * v.W) - (B.xw * v.Y) + (B.yw * v.X) - (r.xyzw * v.Z);
    f64 q_xzw = (B.xz * v.W) - (B.xw * v.Z) + (B.zw * v.X) + (r.xyzw * v.Y);
    f64 q_yzw = (B.yz * v.W) - (B.yw * v.Z) + (B.zw * v.Y) - (r.xyzw * v.X);

    hmm_vec4 result = {};
    result.X = (r.s * q.X) + (q.Y * B.xy) + (q.Z * B.xz) + (q.W * B.xw) + (q_xyz * B.yz) + (q_xyw * B.yw)
               + (q_xzw * B.zw) - (q_yzw * r.xyzw);

    result.Y = (r.s * q.Y) - (q.X * B.xy) + (q.Z * B.yz) + (q.W * B.yw) - (q_xyz * B.xz) - (q_xyw * B.xw)
               + (q_yzw * B.zw) + (q_xzw * r.xyzw);

    result.Z = (r.s * q.Z) - (q.X * B.xz) - (q.Y * B.yz) + (q.W * B.zw) + (q_xyz * B.xy) - (q_xzw * B.xw)
               - (q_yzw * B.yw) - (q_xyw * r.xyzw);

    result.W = (r.s * q.W) - (q.X * B.xw) - (q.Y * B.yw) - (q.Z * B.zw) + (q_xyw * B.xy) + (q_xzw * B.xz)
               + (q_yzw * B.yz) + (q_xyz * r.xyzw);

    return result;
}

inline Rotor4 reverse(const Rotor4& r) {
    const auto& B = r.B;
    Rotor4 result = {};
    result.s = r.s;
    result.B.xy = -B.xy;
    result.B.xz = -B.xz;
    result.B.xw = -B.xw;
    result.B.yz = -B.yz;
    result.B.yw = -B.yw;
    result.B.zw = -B.zw;
    result.xyzw = -r.xyzw;
    return result;
}

// Rotate a rotor `a` by a rotor `r`
inline Rotor4 rotate(const Rotor4& r, const Rotor4& a) {
    return r * a * reverse(r);
}

inline Mat5 to_mat5(const Rotor4& r) {
    hmm_vec4 v_x = rotate(r, vec4(1, 0, 0, 0));
    hmm_vec4 v_y = rotate(r, vec4(0, 1, 0, 0));
    hmm_vec4 v_z = rotate(r, vec4(0, 0, 1, 0));
    hmm_vec4 v_w = rotate(r, vec4(0, 0, 0, 1));
    return mat5(vec5(v_x, 0), vec5(v_y, 0), vec5(v_z, 0), vec5(v_w, 0), vec5(0, 0, 0, 0, 1));
}

inline Rotor4 euler_to_rotor(const Bivec4& B) {
    Rotor4 result = rotor4(B.xy, outer(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0)))
                    * rotor4(B.xz, outer(vec4(1, 0, 0, 0), vec4(0, 0, 1, 0)))
                    * rotor4(B.xw, outer(vec4(1, 0, 0, 0), vec4(0, 0, 0, 1)))
                    * rotor4(B.yz, outer(vec4(0, 1, 0, 0), vec4(0, 0, 1, 0)))
                    * rotor4(B.yw, outer(vec4(0, 1, 0, 0), vec4(0, 0, 0, 1)))
                    * rotor4(B.zw, outer(vec4(0, 0, 1, 0), vec4(0, 0, 0, 1)));

    return result;
}

inline Bivec4 rotor_to_euler(const Rotor4& r) {
    Bivec4 result = {};
    // f64 a = acos(r.s);
    // f64 sin_a = sin(a);
    result = r.B;
    return result;
}

// =========

inline Mat5 rotate_euler(const Bivec4& B) {
    return
            //  XY
            mat5(vec5(cos(B.xy), -sin(B.xy), 0, 0, 0), vec5(sin(B.xy), cos(B.xy), 0, 0, 0), vec5(0, 0, 1, 0, 0),
                 vec5(0, 0, 0, 1, 0), vec5(0, 0, 0, 0, 1))

            // XZ
            * mat5(vec5(cos(-B.xz), 0, sin(-B.xz), 0, 0), vec5(0, 1, 0, 0, 0), vec5(-sin(-B.xz), 0, cos(-B.xz), 0, 0),
                   vec5(0, 0, 0, 1, 0), vec5(0, 0, 0, 0, 1))

            // XW
            * mat5(vec5(cos(B.xw), 0, 0, -sin(B.xw), 0), vec5(0, 1, 0, 0, 0), vec5(0, 0, 1, 0, 0),
                   vec5(sin(B.xw), 0, 0, cos(B.xw), 0), vec5(0, 0, 0, 0, 1))

            // YZ
            * mat5(vec5(1, 0, 0, 0, 0), vec5(0, cos(B.yz), -sin(B.yz), 0, 0), vec5(0, sin(B.yz), cos(B.yz), 0, 0),
                   vec5(0, 0, 0, 1, 0), vec5(0, 0, 0, 0, 1))

            // YW
            * mat5(vec5(1, 0, 0, 0, 0), vec5(0, cos(B.yw), 0, sin(B.yw), 0), vec5(0, 0, 1, 0, 0),
                   vec5(0, -sin(B.yw), 0, cos(B.yw), 0), vec5(0, 0, 0, 0, 1))

            // ZW
            * mat5(vec5(1, 0, 0, 0, 0), vec5(0, 1, 0, 0, 0), vec5(0, 0, cos(B.zw), sin(B.zw), 0),
                   vec5(0, 0, -sin(B.zw), cos(B.zw), 0), vec5(0, 0, 0, 0, 1));
}
} // namespace four
