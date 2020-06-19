#pragma once

#include <stdlib.h>

#include <functional>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

namespace four {

template <class T>
void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

const float flt_default_epsilon = 0.000001f;

bool float_eq(float a, float b, float epsilon);

} // namespace four
