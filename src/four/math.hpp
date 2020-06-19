#pragma once

#include <four/utility.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <loguru.hpp>

#include <stdlib.h>

namespace four {

static_assert(sizeof(glm::dvec2) == sizeof(f64) * 2);
static_assert(sizeof(glm::dvec3) == sizeof(f64) * 3);
static_assert(sizeof(glm::dvec4) == sizeof(f64) * 4);
static_assert(sizeof(glm::dmat3) == sizeof(f64) * 3 * 3);
static_assert(sizeof(glm::dmat4) == sizeof(f64) * 4 * 4);

inline f64 sq(f64 x) {
    return x * x;
}

struct Vec5 {
    CXX_EXTENSION union {
        struct {
            f64 x, y, z, w, v;
        };
        f64 elements[5];
    };

    Vec5() = default;
    Vec5(f64 x, f64 y, f64 z, f64 w, f64 v) noexcept : x(x), y(y), z(z), w(w), v(v) {}
    Vec5(const glm::dvec4& vec, f64 v) noexcept : x(vec.x), y(vec.y), z(vec.z), w(vec.w), v(v) {}

    f64& operator[](size_t index) {
        return elements[index];
    }

    f64 operator[](size_t index) const {
        return elements[index];
    }
};

struct Mat5 {
    union {
        Vec5 columns[5];
        f64 elements[5][5];
    };

    Mat5() = default;
    Mat5(const Vec5& column0, const Vec5& column1, const Vec5& column2, const Vec5& column3,
         const Vec5& column4) noexcept
            : columns{column0, column1, column2, column3, column4} {}

    Vec5& operator[](size_t index) {
        return columns[index];
    }

    const Vec5& operator[](size_t index) const {
        return columns[index];
    }
};

inline glm::dvec4 to_vec4(const Vec5& v) {
    return glm::dvec4(v.x, v.y, v.z, v.w);
}

inline Vec5 operator*(const Vec5& v, f64 s) {
    return Vec5(v.x * s, v.y * s, v.z * s, v.w * s, v.v * s);
}

inline Vec5 operator*(f64 s, const Vec5& v) {
    return v * s;
}

inline Vec5 operator*(const Mat5& m, const Vec5& v) {
    Vec5 result;

    for (size_t r = 0; r < 5; r++) {
        f64 sum = 0;
        for (size_t c = 0; c < 5; c++) {
            sum += m[c][r] * v[c];
        }
        result[r] = sum;
    }

    return result;
}

inline Mat5 operator*(const Mat5& m1, const Mat5& m2) {
    Mat5 result;

    for (size_t c = 0; c < 5; c++) {
        for (size_t r = 0; r < 5; r++) {
            f64 sum = 0;
            for (size_t current_matrix = 0; current_matrix < 5; current_matrix++) {
                sum += m1[current_matrix][r] * m2[c][current_matrix];
            }
            result[c][r] = sum;
        }
    }

    return result;
}

inline glm::dvec3 transform(const glm::dmat4& m, const glm::dvec3& v) {
    return glm::dvec3(m * glm::dvec4(v, 1));
}

inline glm::dmat4 translate(const glm::dvec3& v) {
    return glm::translate(glm::dmat4(1.0), v);
}

inline glm::dvec4 transform(const Mat5& m, const glm::dvec4& v) {
    return to_vec4(m * Vec5(v, 1));
}

inline glm::dvec4 cross(const glm::dvec4& u, const glm::dvec4& v, const glm::dvec4& w) {
    glm::dmat3 m1 = glm::dmat3(glm::dvec3(u.y, v.y, w.y), glm::dvec3(u.z, v.z, w.z), glm::dvec3(u.w, v.w, w.w));
    glm::dmat3 m2 = glm::dmat3(glm::dvec3(u.x, v.x, w.x), glm::dvec3(u.z, v.z, w.z), glm::dvec3(u.w, v.w, w.w));
    glm::dmat3 m3 = glm::dmat3(glm::dvec3(u.x, v.x, w.x), glm::dvec3(u.y, v.y, w.y), glm::dvec3(u.w, v.w, w.w));
    glm::dmat3 m4 = glm::dmat3(glm::dvec3(u.x, v.x, w.x), glm::dvec3(u.y, v.y, w.y), glm::dvec3(u.z, v.z, w.z));
    return glm::dvec4(glm::determinant(m1), -glm::determinant(m2), glm::determinant(m3), -glm::determinant(m4));
}

inline Mat5 translate(const glm::dvec4& v) {
    return Mat5(Vec5(1, 0, 0, 0, 0), Vec5(0, 1, 0, 0, 0), Vec5(0, 0, 1, 0, 0), Vec5(0, 0, 0, 1, 0),
                Vec5(v.x, v.y, v.z, v.w, 1));
}

inline Mat5 scale(const glm::dvec4& v) {
    return Mat5(Vec5(v.x, 0, 0, 0, 0), Vec5(0, v.y, 0, 0, 0), Vec5(0, 0, v.z, 0, 0), Vec5(0, 0, 0, v.w, 0),
                Vec5(0, 0, 0, 0, 1));
}

inline Mat5 look_at(const glm::dvec4& eye, const glm::dvec4& target, const glm::dvec4& up, const glm::dvec4& over) {
    Mat5 m_t = translate(-1.0 * eye);
    glm::dvec4 f = glm::normalize(eye - target);
    glm::dvec4 l = glm::normalize(cross(up, over, f));
    glm::dvec4 u = glm::normalize(cross(over, l, f));
    glm::dvec4 o = cross(f, l, u);
    Mat5 m_r = Mat5(Vec5(l.x, u.x, o.x, f.x, 0), Vec5(l.y, u.y, o.y, f.y, 0), Vec5(l.z, u.z, o.z, f.z, 0),
                    Vec5(l.w, u.w, o.w, f.w, 0), Vec5(0, 0, 0, 0, 1));
    return m_r * m_t;
}

inline Mat5 look_at_inverse(const glm::dvec4& eye, const glm::dvec4& target, const glm::dvec4& up,
                            const glm::dvec4& over) {
    Mat5 m_t = translate(eye);
    glm::dvec4 f = glm::normalize(eye - target);
    glm::dvec4 l = glm::normalize(cross(up, over, f));
    glm::dvec4 u = glm::normalize(cross(over, l, f));
    glm::dvec4 o = cross(f, l, u);
    Mat5 m_r = Mat5(Vec5(l, 0), Vec5(u, 0), Vec5(o, 0), Vec5(f, 0), Vec5(0, 0, 0, 0, 1));
    return m_t * m_r;
}

inline glm::dvec4 project_orthographic(const Vec5& v, f64 near) {
    DCHECK_GT_F(near, 0.0);
    DCHECK_LE_F(v.w, -near);
    return glm::dvec4(v.x, v.y, v.z, std::abs(v.w - (-near)));
}

inline glm::dvec4 project_perspective(const Vec5& v, f64 near) {
    DCHECK_GT_F(near, 0.0);
    DCHECK_LE_F(v.w, -near);
    f64 d = near / -v.w;
    glm::dvec4 intersect = d * to_vec4(v);
    return glm::dvec4(intersect.x, intersect.y, intersect.z, glm::length(intersect - to_vec4(v)));
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
inline Bivec3 outer(const glm::dvec3& a, const glm::dvec3& b) {
    Bivec3 result = {};
    result.xy = (a.x * b.y) - (a.y * b.x);
    result.xz = (a.x * b.z) - (a.z * b.x);
    result.yz = (a.y * b.z) - (a.z * b.y);
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

inline Rotor3 rotor3(const glm::dvec3& a, const glm::dvec3& b) {
    Rotor3 result = {};
    result.s = glm::dot(a, b);

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

inline glm::dvec3 rotate(const Rotor3& r, const glm::dvec3& v) {
    const auto& B = r.B;

    // (ba)v -- vector part
    glm::dvec3 q = {};
    q.x = (r.s * v.x) + (v.y * B.xy) + (v.z * B.xz);
    q.y = (r.s * v.y) - (v.x * B.xy) + (v.z * B.yz);
    q.z = (r.s * v.z) - (v.x * B.xz) - (v.y * B.yz);

    // (ba)v -- trivector part
    f64 q_xyz = (-v.x * B.yz) + (v.y * B.xz) - (v.z * B.xy);

    glm::dvec3 result = {};
    result.x = (r.s * q.x) + (q.y * B.xy) + (q.z * B.xz) - (q_xyz * B.yz);
    result.y = (r.s * q.y) - (q.x * B.xy) + (q_xyz * B.xz) + (q.z * B.yz);
    result.z = (r.s * q.z) - (q_xyz * B.xy) - (q.x * B.xz) - (q.y * B.yz);

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

inline glm::dmat4 to_mat4(const Rotor3& r) {
    glm::dvec3 v_x = rotate(r, glm::dvec3(1, 0, 0));
    glm::dvec3 v_y = rotate(r, glm::dvec3(0, 1, 0));
    glm::dvec3 v_z = rotate(r, glm::dvec3(0, 0, 1));
    return glm::dmat4(glm::dvec4(v_x, 0), glm::dvec4(v_y, 0), glm::dvec4(v_z, 0), glm::dvec4(0, 0, 0, 1));
}

// =========

// 4D Rotors
// =========

struct Bivec4 {
    CXX_EXTENSION union {
        struct {
            f64 xy, xz, xw, yz, yw, zw;
        };
        f64 elements[6];
    };

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
inline Bivec4 outer(const glm::dvec4& a, const glm::dvec4& b) {
    Bivec4 result = {};
    result.xy = (a.x * b.y) - (a.y * b.x);
    result.xz = (a.x * b.z) - (a.z * b.x);
    result.xw = (a.x * b.w) - (a.w * b.x);
    result.yz = (a.y * b.z) - (a.z * b.y);
    result.yw = (a.y * b.w) - (a.w * b.y);
    result.zw = (a.z * b.w) - (a.w * b.z);
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

inline Rotor4 rotor4(const glm::dvec4& a, const glm::dvec4& b) {
    Rotor4 result = {};
    result.s = glm::dot(a, b);
    result.B.xy = (b.x * a.y) - (b.y * a.x);
    result.B.xz = (b.x * a.z) - (b.z * a.x);
    result.B.xw = (b.x * a.w) - (b.w * a.x);
    result.B.yz = (b.y * a.z) - (b.z * a.y);
    result.B.yw = (b.y * a.w) - (b.w * a.y);
    result.B.zw = (b.z * a.w) - (b.w * a.z);
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

inline glm::dvec4 rotate(const Rotor4& r, const glm::dvec4& v) {
    const auto& B = r.B;

    // (ba)v -- vector part
    glm::dvec4 q = {};
    q.x = (r.s * v.x) + (B.xy * v.y) + (B.xz * v.z) + (B.xw * v.w);
    q.y = (r.s * v.y) - (B.xy * v.x) + (B.yz * v.z) + (B.yw * v.w);
    q.z = (r.s * v.z) - (B.xz * v.x) - (B.yz * v.y) + (B.zw * v.w);
    q.w = (r.s * v.w) - (B.xw * v.x) - (B.yw * v.y) - (B.zw * v.z);

    // (ba)v -- trivector part
    f64 q_xyz = (B.xy * v.z) - (B.xz * v.y) + (B.yz * v.x) + (r.xyzw * v.w);
    f64 q_xyw = (B.xy * v.w) - (B.xw * v.y) + (B.yw * v.x) - (r.xyzw * v.z);
    f64 q_xzw = (B.xz * v.w) - (B.xw * v.z) + (B.zw * v.x) + (r.xyzw * v.y);
    f64 q_yzw = (B.yz * v.w) - (B.yw * v.z) + (B.zw * v.y) - (r.xyzw * v.x);

    glm::dvec4 result = {};
    result.x = (r.s * q.x) + (q.y * B.xy) + (q.z * B.xz) + (q.w * B.xw) + (q_xyz * B.yz) + (q_xyw * B.yw)
               + (q_xzw * B.zw) - (q_yzw * r.xyzw);

    result.y = (r.s * q.y) - (q.x * B.xy) + (q.z * B.yz) + (q.w * B.yw) - (q_xyz * B.xz) - (q_xyw * B.xw)
               + (q_yzw * B.zw) + (q_xzw * r.xyzw);

    result.z = (r.s * q.z) - (q.x * B.xz) - (q.y * B.yz) + (q.w * B.zw) + (q_xyz * B.xy) - (q_xzw * B.xw)
               - (q_yzw * B.yw) - (q_xyw * r.xyzw);

    result.w = (r.s * q.w) - (q.x * B.xw) - (q.y * B.yw) - (q.z * B.zw) + (q_xyw * B.xy) + (q_xzw * B.xz)
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
    glm::dvec4 v_x = rotate(r, glm::dvec4(1, 0, 0, 0));
    glm::dvec4 v_y = rotate(r, glm::dvec4(0, 1, 0, 0));
    glm::dvec4 v_z = rotate(r, glm::dvec4(0, 0, 1, 0));
    glm::dvec4 v_w = rotate(r, glm::dvec4(0, 0, 0, 1));
    return Mat5(Vec5(v_x, 0), Vec5(v_y, 0), Vec5(v_z, 0), Vec5(v_w, 0), Vec5(0, 0, 0, 0, 1));
}

inline Rotor4 euler_to_rotor(const Bivec4& B) {
    Rotor4 result = rotor4(B.xy, outer(glm::dvec4(1, 0, 0, 0), glm::dvec4(0, 1, 0, 0)))
                    * rotor4(B.xz, outer(glm::dvec4(1, 0, 0, 0), glm::dvec4(0, 0, 1, 0)))
                    * rotor4(B.xw, outer(glm::dvec4(1, 0, 0, 0), glm::dvec4(0, 0, 0, 1)))
                    * rotor4(B.yz, outer(glm::dvec4(0, 1, 0, 0), glm::dvec4(0, 0, 1, 0)))
                    * rotor4(B.yw, outer(glm::dvec4(0, 1, 0, 0), glm::dvec4(0, 0, 0, 1)))
                    * rotor4(B.zw, outer(glm::dvec4(0, 0, 1, 0), glm::dvec4(0, 0, 0, 1)));

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
    Bivec4 B_ = B;
    B_.xy = -B.xy;
    B_.xz = -B.xz;
    B_.xw = -B.xw;
    B_.yz = -B.yz;
    B_.yw = -B.yw;
    B_.zw = -B.zw;

    return
            //  XY
            Mat5(Vec5(cos(B_.xy), -sin(B_.xy), 0, 0, 0), Vec5(sin(B_.xy), cos(B_.xy), 0, 0, 0), Vec5(0, 0, 1, 0, 0),
                 Vec5(0, 0, 0, 1, 0), Vec5(0, 0, 0, 0, 1))

            // XZ
            * Mat5(Vec5(cos(-B_.xz), 0, sin(-B_.xz), 0, 0), Vec5(0, 1, 0, 0, 0),
                   Vec5(-sin(-B_.xz), 0, cos(-B_.xz), 0, 0), Vec5(0, 0, 0, 1, 0), Vec5(0, 0, 0, 0, 1))

            // XW
            * Mat5(Vec5(cos(B_.xw), 0, 0, -sin(B_.xw), 0), Vec5(0, 1, 0, 0, 0), Vec5(0, 0, 1, 0, 0),
                   Vec5(sin(B_.xw), 0, 0, cos(B_.xw), 0), Vec5(0, 0, 0, 0, 1))

            // YZ
            * Mat5(Vec5(1, 0, 0, 0, 0), Vec5(0, cos(B_.yz), -sin(B_.yz), 0, 0), Vec5(0, sin(B_.yz), cos(B_.yz), 0, 0),
                   Vec5(0, 0, 0, 1, 0), Vec5(0, 0, 0, 0, 1))

            // YW
            * Mat5(Vec5(1, 0, 0, 0, 0), Vec5(0, cos(B_.yw), 0, sin(B_.yw), 0), Vec5(0, 0, 1, 0, 0),
                   Vec5(0, -sin(B_.yw), 0, cos(B_.yw), 0), Vec5(0, 0, 0, 0, 1))

            // ZW
            * Mat5(Vec5(1, 0, 0, 0, 0), Vec5(0, 1, 0, 0, 0), Vec5(0, 0, cos(B_.zw), sin(B_.zw), 0),
                   Vec5(0, 0, -sin(B_.zw), cos(B_.zw), 0), Vec5(0, 0, 0, 0, 1));
}
} // namespace four
