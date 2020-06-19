#include <four/math.hpp>

namespace four {

Vec5 operator*(Mat5 m, const Vec5& v) {
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

Mat5 operator*(Mat5 m1, const Mat5& m2) {
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

hmm_vec4 vec4(const Vec5& v) {
    return HMM_Vec4(v[0], v[1], v[2], v[3]);
}

Vec5 vec5(hmm_vec4 vec, float v) {
    return {vec[0], vec[1], vec[2], vec[3], v};
}
} // namespace four
