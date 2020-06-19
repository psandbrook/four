#pragma once

#include <cfloat>
#include <cmath>
#include <functional>
#include <stdint.h>
#include <string.h>
#include <string>
#include <unordered_set>

#include <loguru.hpp>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof(*(arr)))
#define CXX_EXTENSION __extension__

#ifdef FOUR_DEBUG
#    define DPRINT(...) DRAW_LOG_F(INFO, __VA_ARGS__)

// clang-format off
#    define DEXPR(expr) ::four::dexpr(#expr, (expr))
// clang-format on

#else
#    define DPRINT(...)
#    define DEXPR(expr)
#endif

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

using loguru::strprintf;

template <class T>
std::string dstr(const T& value);

template <>
inline std::string dstr(const u8& value) {
    return strprintf("%hhu", value);
}

template <>
inline std::string dstr(const u16& value) {
    return strprintf("%hu", value);
}

template <>
inline std::string dstr(const u32& value) {
    return strprintf("%u", value);
}

template <>
inline std::string dstr(const u64& value) {
    return strprintf("%llu", (unsigned long long)value);
}

template <>
inline std::string dstr(const s8& value) {
    return strprintf("%hhi", value);
}

template <>
inline std::string dstr(const s16& value) {
    return strprintf("%hi", value);
}

template <>
inline std::string dstr(const s32& value) {
    return strprintf("%i", value);
}

template <>
inline std::string dstr(const s64& value) {
    return strprintf("%lli", (long long)value);
}

template <>
inline std::string dstr(const f32& value) {
    return strprintf("%.7f", value);
}

template <>
inline std::string dstr(const f64& value) {
    return strprintf("%.16f", value);
}

template <class T>
inline void dexpr(const char* name, const T& value) {
    DPRINT("%s: %s", name, dstr(value).c_str());
}

inline void dline() {
    DPRINT(" ");
}

template <class T, size_t N>
struct BoundedVector {
    size_t len;
    T data[N];

    BoundedVector() noexcept : len(0) {}

    T* begin() noexcept {
        return data;
    }

    T* end() noexcept {
        return data + len;
    }

    const T* cbegin() const noexcept {
        return data;
    }

    const T* cend() const noexcept {
        return data + len;
    }

    T& operator[](size_t index) noexcept {
        DCHECK_LT_F(index, len);
        return data[index];
    }

    const T& operator[](size_t index) const noexcept {
        DCHECK_LT_F(index, len);
        return data[index];
    }

    void push_back(const T& value) {
        DCHECK_LT_F(len, N);
        data[len] = value;
        len++;
    }

    void push_back(T&& value) {
        DCHECK_LT_F(len, N);
        data[len] = std::move(value);
        len++;
    }
};

constexpr f64 default_epsilon = 0.00000000000001;

inline bool float_eq(f64 a, f64 b, f64 epsilon = default_epsilon) {
    if (a < 1.0 && b < 1.0) {
        return std::abs(a - b) <= epsilon;
    } else {
        return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
    }
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
    vec.push_back(std::move(value));
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

template <class T, size_t N>
inline bool contains(const BoundedVector<T, N>& vec, const T& value) {
    return std::find(vec.cbegin(), vec.cend(), value) != vec.cend();
}

template <class K, class V, class Hash, class Equals>
inline bool has_key(const std::unordered_map<K, V, Hash, Equals>& map, const K& value) {
    return map.find(value) != map.cend();
}

inline bool c_str_eq(const char* lhs, const char* rhs) {
    return strcmp(lhs, rhs) == 0;
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
        return c_str_eq(lhs, rhs);
    }
};
} // namespace four
