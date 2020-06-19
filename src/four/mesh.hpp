#pragma once

#include <four/math.hpp>
#include <four/utility.hpp>

#include <stdint.h>
#include <stdlib.h>

#include <vector>

namespace four {

struct Edge {
    CXX_EXTENSION union {
        struct {
            u32 v0;
            u32 v1;
        };
        u32 vertices[2];
    };

    Edge() = default;
    Edge(u32 v0, u32 v1) noexcept : v0(v0), v1(v1) {}
};

using Face = std::vector<u32>;
using Cell = std::vector<u32>;

struct Mesh4 {
    std::vector<glm::dvec4> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Cell> cells;
};

struct FaceHash {
private:
    mutable std::vector<u32> x_;

public:
    size_t operator()(const std::vector<u32>& x) const;
};

struct FaceEquals {
    bool operator()(const std::vector<u32>& lhs, const std::vector<u32>& rhs) const;
};

using CellHash = FaceHash;
using CellEquals = FaceEquals;

bool operator==(const Edge& lhs, const Edge& rhs);

bool save_mesh_to_file(const Mesh4& mesh, const char* path);
Mesh4 load_mesh_from_file(const char* path);

} // namespace four

namespace std {

template <>
struct hash<four::Edge> {
    size_t operator()(const four::Edge& x) const {
        size_t hash0 = 0;
        four::hash_combine(hash0, x.v0);
        four::hash_combine(hash0, x.v1);

        size_t hash1 = 0;
        four::hash_combine(hash1, x.v1);
        four::hash_combine(hash1, x.v0);

        return std::min(hash0, hash1);
    }
};

template <>
struct hash<glm::dvec4> {
    size_t operator()(const glm::dvec4& v) const {
        size_t hash = 0;
        for (four::s32 i = 0; i < 4; i++) {
            four::hash_combine(hash, v[i]);
        }
        return hash;
    }
};
} // namespace std
