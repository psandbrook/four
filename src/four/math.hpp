#pragma once

#include <HandmadeMath.h>

#include <stdlib.h>

namespace four {

struct Mat5 {
    float elements[5][5];
};

struct Vec5 {
    float elements[5];

    float& operator[](size_t index) {
        return elements[index];
    }

    float operator[](size_t index) const {
        return elements[index];
    }
};

Vec5 operator*(Mat5 m, const Vec5& v);
Mat5 operator*(Mat5 m1, const Mat5& m2);

hmm_vec4 vec4(const Vec5& v);
Vec5 vec5(hmm_vec4 vec, float v);

} // namespace four
