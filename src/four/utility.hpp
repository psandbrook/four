#pragma once

#include <stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>
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

template <class T>
bool contains(const std::unordered_set<T>& set, const T& value) {
    return set.find(value) != set.cend();
}

template <class K, class V>
bool has_key(const std::unordered_map<K, V>& map, const K& value) {
    return map.find(value) != map.cend();
}

inline bool float_eq(float a, float b, float epsilon = FLT_EPSILON) {
    return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
}

inline bool float_eq_abs(float a, float b, float epsilon = FLT_EPSILON) {
    return std::abs(a - b) <= epsilon;
}

} // namespace four
