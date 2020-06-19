#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof(*(arr)))

namespace four {

// Shorthands for integer and floating point types
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

template <class T>
void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class T>
bool contains(const std::vector<T>& vec, const T& value) {
    return std::find(vec.cbegin(), vec.cend(), value) != vec.cend();
}

template <class T>
bool contains(const std::unordered_set<T>& set, const T& value) {
    return set.find(value) != set.cend();
}

template <class K, class V>
bool has_key(const std::unordered_map<K, V>& map, const K& value) {
    return map.find(value) != map.cend();
}

inline bool float_eq(f64 a, f64 b, f64 epsilon = DBL_EPSILON) {
    if (a < 1.0 && b < 1.0) {
        return std::abs(a - b) <= epsilon;
    } else {
        return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
    }
}
} // namespace four
