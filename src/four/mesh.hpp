#pragma once

#include <four/math.hpp>
#include <four/utility.hpp>

#include <stdint.h>
#include <stdlib.h>

#include <vector>

namespace four {

struct Edge {

    // `v0` and `v1` are indices of a `Mesh4`'s `vertices` vector.
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

// A `Face` is an unordered vector of indices of a `Mesh4`'s `edges` vector.
using Face = std::vector<u32>;

// A `Cell` is an unordered vector of indices of a `Mesh4`'s `faces` vector.
using Cell = std::vector<u32>;

struct Mesh4 {

    struct Tet {

        // Index of the `cells` vector.
        u32 cell;

        // Array of indices of the `tet_vertices` vector.
        u32 vertices[4];
    };

    std::string name;

    // Vector of four-dimensional points that represent the vertices of the
    // mesh.
    std::vector<glm::dvec4> vertices;

    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Cell> cells;

    // Vector of four-dimensional points that represent the vertices of the
    // tetrahedralization of the mesh.
    std::vector<glm::dvec4> tet_vertices;

    std::vector<Tet> tets;
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

// Calculate the tetrahedralization of `mesh`, filling in the `tet_vertices` and
// `tets` fields.
void tetrahedralize(Mesh4& mesh);

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
