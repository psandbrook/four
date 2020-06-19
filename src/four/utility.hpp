#pragma once

#include <cfloat>
#include <cmath>
#include <functional>
#include <stdint.h>
#include <string.h>
#include <unordered_set>

#include <loguru.hpp>

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

constexpr f64 default_epsilon = 0.00000000000001;

inline bool float_eq(f64 a, f64 b, f64 epsilon = default_epsilon) {
    if (a < 1.0 && b < 1.0) {
        return std::abs(a - b) <= epsilon;
    } else {
        return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
    }
}

template <class T>
struct Slice {
    size_t len;
    T* data;

    Slice() noexcept {}
    Slice(size_t len, T* data) noexcept : len(len), data(data) {}

    T& operator[](size_t index) noexcept {
        CHECK_LT_F(index, len);
        return data[index];
    }

    const T& operator[](size_t index) const noexcept {
        CHECK_LT_F(index, len);
        return data[index];
    }
};

#define AS_SLICE(arr) (four::Slice(ARRAY_SIZE((arr)), (arr)))

template <class T>
inline Slice<const T> c_slice(size_t len, const T* data) {
    return Slice<const T>(len, data);
}

template <class T>
inline void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class T>
inline size_t insert_back(std::vector<T>& vec, const T& value) {
    size_t index = vec.size();
    vec.push_back(value);
    return index;
}

template <class T>
inline size_t insert_back(std::vector<T>& vec, T&& value) {
    size_t index = vec.size();
    vec.push_back(std::forward<T>(value));
    return index;
}

template <class T>
inline bool contains(const std::vector<T>& vec, const T& value) {
    return std::find(vec.cbegin(), vec.cend(), value) != vec.cend();
}

template <class T>
inline bool contains(const std::unordered_set<T>& set, const T& value) {
    return set.find(value) != set.cend();
}

template <class T>
inline bool contains(Slice<const T> slice, const T& value) {
    const T* begin = slice.data;
    const T* end = &slice.data[slice.len];
    return std::find(begin, end, value) != end;
}

template <class T>
inline bool contains(Slice<T> slice, const T& value) {
    Slice<const T> slice_(slice.len, slice.data);
    return contains(slice_, value);
}

template <class K, class V, class Hash, class Equals>
inline bool has_key(const std::unordered_map<K, V, Hash, Equals>& map, const K& value) {
    return map.find(value) != map.cend();
}

struct CStrHash {
    size_t operator()(const char* x) const {
        size_t hash = 0;
        for (s32 i = 0; x[i] != '\0'; i++) {
            hash_combine(hash, x[i]);
        }
        return hash;
    }
};

struct CStrEquals {
    bool operator()(const char* lhs, const char* rhs) const {
        return strcmp(lhs, rhs) == 0;
    }
};
} // namespace four
