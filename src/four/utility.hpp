#pragma once

#include <stdlib.h>

#include <algorithm>
#include <functional>
#include <vector>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

namespace four {

template <class T>
void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class T>
bool contains(const std::vector<T>& vec, const T& value) {
    return std::find(vec.cbegin(), vec.cend(), value) != vec.cend();
}

const float flt_default_epsilon = 0.000001f;

bool float_eq(float a, float b, float epsilon);

} // namespace four
