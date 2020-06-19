#pragma once

#include <four/utility.hpp>

#include <HandmadeMath.h>

#include <stdint.h>
#include <stdlib.h>

#include <vector>

namespace four {

struct Edge {
    uint32_t v1;
    uint32_t v2;
};

using Face = std::vector<uint32_t>;
using Cell = std::vector<uint32_t>;

// TODO: Use double-precision or single-precision floating point?
// Double-precision may be faster (profile!).
struct Mesh4 {
    std::vector<hmm_vec4> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Cell> cells;
};

struct FaceHash {
    size_t operator()(const std::vector<uint32_t>& x) const;
};

struct FaceEquals {
    bool operator()(const std::vector<uint32_t>& lhs, const std::vector<uint32_t>& rhs) const;
};

using CellHash = FaceHash;
using CellEquals = FaceEquals;

bool operator==(const Edge& lhs, const Edge& rhs);

} // namespace four

namespace std {

template <>
struct hash<four::Edge> {
    size_t operator()(const four::Edge& x) const {
        size_t hash0 = 0;
        four::hash_combine(hash0, x.v1);
        four::hash_combine(hash0, x.v2);

        size_t hash1 = 0;
        four::hash_combine(hash1, x.v2);
        four::hash_combine(hash1, x.v1);

        return std::min(hash0, hash1);
    }
};
} // namespace std
