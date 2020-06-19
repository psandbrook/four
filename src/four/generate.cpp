#include <four/generate.hpp>

#include <four/utility.hpp>

#include <HandmadeMath.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace four {

namespace {

// clang-format off
const hmm_vec4 n5cell_vertices[] = {
    { 1.0/sqrt(10.0),     1.0/sqrt(6.0),  1.0/sqrt(3.0),  1.0},
    { 1.0/sqrt(10.0),     1.0/sqrt(6.0),  1.0/sqrt(3.0), -1.0},
    { 1.0/sqrt(10.0),     1.0/sqrt(6.0), -2.0/sqrt(3.0),  0.0},
    { 1.0/sqrt(10.0),    -sqrt(3.0/2.0),  0.0,            0.0},
    {-2.0*sqrt(2.0/5.0),  0.0,            0.0,            0.0},
};
// clang-format on

const f64 n5cell_edge_length = 2.0;
const s32 n5cell_edges_per_face = 3;
const s32 n5cell_faces_per_cell = 4;

// clang-format off
const hmm_vec4 tesseract_vertices[] = {
    {-1, -1, -1, -1},
    {-1, -1,  1, -1},
    {-1, -1, -1,  1},
    {-1, -1,  1,  1},
    { 1, -1, -1, -1},
    { 1, -1,  1, -1},
    { 1, -1, -1,  1},
    { 1, -1,  1,  1},
    {-1,  1, -1, -1},
    {-1,  1,  1, -1},
    {-1,  1, -1,  1},
    {-1,  1,  1,  1},
    { 1,  1, -1, -1},
    { 1,  1,  1, -1},
    { 1,  1, -1,  1},
    { 1,  1,  1,  1},
};
// clang-format on

const f64 tesseract_edge_length = 2.0;
const s32 tesseract_edges_per_face = 4;
const s32 tesseract_faces_per_cell = 6;

// clang-format off
const hmm_vec4 n16cell_vertices[] = {
    { 1,  0,  0,  0},
    {-1,  0,  0,  0},
    { 0,  1,  0,  0},
    { 0, -1,  0,  0},
    { 0,  0,  1,  0},
    { 0,  0, -1,  0},
    { 0,  0,  0,  1},
    { 0,  0,  0, -1},
};
// clang-format on

const f64 n16cell_edge_length = sqrt(2.0);
const s32 n16cell_edges_per_face = 3;
const s32 n16cell_faces_per_cell = 4;

// clang-format off
const hmm_vec4 n24cell_vertices[] = {
    { 1,  1,  0,  0},
    { 1,  0,  1,  0},
    { 1,  0,  0,  1},
    { 0,  1,  1,  0},
    { 0,  1,  0,  1},
    { 0,  0,  1,  1},
    {-1,  1,  0,  0},
    {-1,  0,  1,  0},
    {-1,  0,  0,  1},
    { 0, -1,  1,  0},
    { 0, -1,  0,  1},
    { 0,  0, -1,  1},
    { 1, -1,  0,  0},
    { 1,  0, -1,  0},
    { 1,  0,  0, -1},
    { 0,  1, -1,  0},
    { 0,  1,  0, -1},
    { 0,  0,  1, -1},
    {-1, -1,  0,  0},
    {-1,  0, -1,  0},
    {-1,  0,  0, -1},
    { 0, -1, -1,  0},
    { 0, -1,  0, -1},
    { 0,  0, -1, -1},
};
// clang-format on

const f64 n24cell_edge_length = sqrt(2.0);
const s32 n24cell_edges_per_face = 3;
const s32 n24cell_faces_per_cell = 8;

bool face_is_valid(const Mesh4& mesh, const std::unordered_set<u32>& face) {
    std::unordered_map<u32, s32> vertices_count;

    for (u32 edge_i : face) {
        const Edge& edge = mesh.edges[edge_i];
        for (u32 vert_i : edge.vertices) {
            if (!has_key(vertices_count, vert_i)) {
                vertices_count.emplace(vert_i, 1);
            } else {
                vertices_count[vert_i] += 1;
                if (vertices_count[vert_i] > 2) {
                    return false;
                }
            }
        }
    }

    for (const auto& entry : vertices_count) {
        if (entry.second != 2) {
            return false;
        }
    }

    return true;
}

bool cell_is_valid(const Mesh4& mesh, const std::unordered_set<u32>& cell) {
    std::unordered_map<u32, s32> edges_count;
    for (u32 face_i : cell) {
        const Face& face = mesh.faces[face_i];
        for (u32 edge_i : face) {
            if (!has_key(edges_count, edge_i)) {
                edges_count.emplace(edge_i, 1);
            } else {
                edges_count[edge_i] += 1;
                if (edges_count[edge_i] > 2) {
                    return false;
                }
            }
        }
    }

    for (const auto& entry : edges_count) {
        if (entry.second != 2) {
            return false;
        }
    }

    return true;
}

Mesh4 generate_mesh4(const hmm_vec4* vertices, s32 n_vertices, f64 edge_length, s32 edges_per_face,
                     s32 faces_per_cell) {
    Mesh4 mesh;

    mesh.vertices = std::vector<hmm_vec4>(vertices, vertices + n_vertices);

    // Calculate edges

    std::unordered_set<Edge> edge_set;

    const f64 edge_length_sq = edge_length * edge_length;

    for (u32 i = 0; i < mesh.vertices.size(); i++) {
        for (u32 j = 0; j < mesh.vertices.size(); j++) {
            if (j != i && float_eq(HMM_LengthSquared(mesh.vertices[j] - mesh.vertices[i]), edge_length_sq)) {
                auto e = edge(i, j);
                edge_set.insert(e);
            }
        }
    }

    mesh.edges = std::vector<Edge>(edge_set.cbegin(), edge_set.cend());

    // Calculate faces

    u32 vertex_edge_n = 0;
    for (const auto& edge : mesh.edges) {
        if (edge.v0 == 0 || edge.v1 == 0) {
            vertex_edge_n++;
        }
    }

    std::vector<u32> vertex_edge_indices;
    vertex_edge_indices.reserve(mesh.vertices.size() * vertex_edge_n);

    for (u32 i = 0; i < mesh.vertices.size(); i++) {
        for (u32 edge_i = 0; edge_i < mesh.edges.size(); edge_i++) {
            const Edge& edge = mesh.edges[edge_i];
            if (edge.v0 == i || edge.v1 == i) {
                vertex_edge_indices.push_back(edge_i);
            }
        }
    }

    assert(vertex_edge_indices.size() == mesh.vertices.size() * vertex_edge_n);

    std::unordered_set<Face, FaceHash, FaceEquals> face_set;
    std::unordered_set<u32> edge_path;

    // Recursive lambda definition
    std::function<void(u32)> fill_face_set;
    fill_face_set = [&](u32 vertex_i) {
        assert(edge_path.size() <= (size_t)edges_per_face);

        if (edge_path.size() == (size_t)edges_per_face) {
            if (face_is_valid(mesh, edge_path)) {
                face_set.emplace(edge_path.cbegin(), edge_path.cend());
            }
        } else {
            for (u32 i = 0; i < vertex_edge_n; i++) {
                u32 edge_i = vertex_edge_indices[vertex_i * vertex_edge_n + i];
                if (!contains(edge_path, edge_i)) {
                    edge_path.insert(edge_i);
                    const Edge& edge = mesh.edges[edge_i];
                    u32 other_vertex_i = edge.v0 == vertex_i ? edge.v1 : edge.v0;
                    fill_face_set(other_vertex_i);
                    edge_path.erase(edge_i);
                }
            }
        }
    };

    for (u32 i = 0; i < mesh.vertices.size(); i++) {
        assert(edge_path.size() == 0);
        fill_face_set(i);
    }

    mesh.faces = std::vector<Face>(face_set.cbegin(), face_set.cend());

    // Calculate cells

    // Calculate the number of adjacent faces per face
    u32 adjacent_faces_n = 0;
    {
        const Face& init_face = mesh.faces[0];
        for (u32 i = 1; i < mesh.faces.size(); i++) {
            const Face& other_face = mesh.faces[i];
            for (u32 edge_i : init_face) {
                if (contains(other_face, edge_i)) {
                    adjacent_faces_n++;
                    break;
                }
            }
        }
    }

    // Calculate all adjacent faces
    std::vector<u32> adjacent_faces;
    adjacent_faces.reserve(mesh.faces.size() * adjacent_faces_n);

    for (u32 i = 0; i < mesh.faces.size(); i++) {
        const Face& current_face = mesh.faces[i];
        for (u32 other_i = 0; other_i < mesh.faces.size(); other_i++) {
            if (other_i != i) {
                const Face& other_face = mesh.faces[other_i];
                for (u32 edge_i : current_face) {
                    if (contains(other_face, edge_i)) {
                        adjacent_faces.push_back(other_i);
                        break;
                    }
                }
            }
        }
    }

    assert(adjacent_faces.size() == mesh.faces.size() * adjacent_faces_n);

    std::unordered_set<Cell, CellHash, CellEquals> cell_set;
    std::unordered_set<u32> face_path;

    // Recursive lambda definition
    std::function<void(u32)> fill_cell_set;
    fill_cell_set = [&](u32 face_i) {
        assert(!contains(face_path, face_i));
        face_path.insert(face_i);
        assert(face_path.size() <= (size_t)faces_per_cell);

        if (face_path.size() == (size_t)faces_per_cell) {
            if (cell_is_valid(mesh, face_path)) {
                cell_set.emplace(face_path.cbegin(), face_path.cend());
            }
        } else {
            for (u32 adj_i = 0; adj_i < adjacent_faces_n; adj_i++) {
                u32 adj_face_i = adjacent_faces[face_i * adjacent_faces_n + adj_i];
                if (!contains(face_path, adj_face_i)) {
                    fill_cell_set(adj_face_i);
                }
            }
        }

        face_path.erase(face_i);
    };

    for (u32 i = 0; i < mesh.faces.size(); i++) {
        assert(face_path.size() == 0);
        fill_cell_set(i);
    }

    mesh.cells = std::vector<Cell>(cell_set.cbegin(), cell_set.cend());

    return mesh;
}
} // namespace

Mesh4 generate_5cell() {
    return generate_mesh4(n5cell_vertices, ARRAY_SIZE(n5cell_vertices), n5cell_edge_length, n5cell_edges_per_face,
                          n5cell_faces_per_cell);
}

Mesh4 generate_tesseract() {
    return generate_mesh4(tesseract_vertices, ARRAY_SIZE(tesseract_vertices), tesseract_edge_length,
                          tesseract_edges_per_face, tesseract_faces_per_cell);
}

Mesh4 generate_16cell() {
    return generate_mesh4(n16cell_vertices, ARRAY_SIZE(n16cell_vertices), n16cell_edge_length, n16cell_edges_per_face,
                          n16cell_faces_per_cell);
}

Mesh4 generate_24cell() {
    return generate_mesh4(n24cell_vertices, ARRAY_SIZE(n24cell_vertices), n24cell_edge_length, n24cell_edges_per_face,
                          n24cell_faces_per_cell);
}
} // namespace four
