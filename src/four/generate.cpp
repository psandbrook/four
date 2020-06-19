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
    { 1.0f/sqrtf(10.0f),      1.0f/sqrtf(6.0f),  1.0f/sqrtf(3.0f),  1.0f},
    { 1.0f/sqrtf(10.0f),      1.0f/sqrtf(6.0f),  1.0f/sqrtf(3.0f), -1.0f},
    { 1.0f/sqrtf(10.0f),      1.0f/sqrtf(6.0f), -2.0f/sqrtf(3.0f),  0.0f},
    { 1.0f/sqrtf(10.0f),     -sqrtf(3.0f/2.0f),  0.0f,              0.0f},
    {-2.0f*sqrtf(2.0f/5.0f),  0.0f,              0.0f,              0.0f},
};
// clang-format on

const float n5cell_edge_length = 2.0f;
const int n5cell_edges_per_face = 3;
const int n5cell_faces_per_cell = 4;

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

const float tesseract_edge_length = 2.0f;
const int tesseract_edges_per_face = 4;
const int tesseract_faces_per_cell = 6;

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

const float n16cell_edge_length = sqrtf(2.0f);
const int n16cell_edges_per_face = 3;
const int n16cell_faces_per_cell = 4;

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

const float n24cell_edge_length = sqrtf(2.0f);
const int n24cell_edges_per_face = 3;
const int n24cell_faces_per_cell = 8;

bool face_is_valid(const Mesh4& mesh, const std::unordered_set<uint32_t>& face) {
    std::unordered_map<uint32_t, int> vertices_count;

    for (uint32_t edge_i : face) {
        const Edge& edge = mesh.edges[edge_i];
        uint32_t vert_indices[] = {edge.v1, edge.v2};
        for (uint32_t vert_i : vert_indices) {
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

bool cell_is_valid(const Mesh4& mesh, const std::unordered_set<uint32_t>& cell) {
    std::unordered_map<uint32_t, int> edges_count;
    for (uint32_t face_i : cell) {
        const Face& face = mesh.faces[face_i];
        for (uint32_t edge_i : face) {
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

Mesh4 generate_mesh4(const hmm_vec4* vertices, int n_vertices, float edge_length, int edges_per_face,
                     int faces_per_cell) {
    Mesh4 mesh;

    mesh.vertices = std::vector<hmm_vec4>(vertices, vertices + n_vertices);

    // Calculate edges

    std::unordered_set<Edge> edge_set;

    const float edge_length_sq = edge_length * edge_length;

    for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
        for (uint32_t j = 0; j < mesh.vertices.size(); j++) {
            if (j != i && float_eq(HMM_LengthSquared(mesh.vertices[j] - mesh.vertices[i]), edge_length_sq)) {
                Edge e = {i, j};
                edge_set.insert(e);
            }
        }
    }

    mesh.edges = std::vector<Edge>(edge_set.cbegin(), edge_set.cend());

    // Calculate faces

    int vertex_edge_n = 0;
    for (const auto& edge : mesh.edges) {
        if (edge.v1 == 0 || edge.v2 == 0) {
            vertex_edge_n++;
        }
    }

    std::vector<uint32_t> vertex_edge_indices;
    vertex_edge_indices.reserve(mesh.vertices.size() * vertex_edge_n);

    for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
        for (uint32_t edge_i = 0; edge_i < mesh.edges.size(); edge_i++) {
            const Edge& edge = mesh.edges[edge_i];
            if (edge.v1 == i || edge.v2 == i) {
                vertex_edge_indices.push_back(edge_i);
            }
        }
    }

    assert(vertex_edge_indices.size() == mesh.vertices.size() * vertex_edge_n);

    std::unordered_set<Face, FaceHash, FaceEquals> face_set;
    std::unordered_set<uint32_t> edge_path;

    // Recursive lambda definition
    std::function<void(uint32_t)> fill_face_set;
    fill_face_set = [&](uint32_t vertex_i) {
        assert(edge_path.size() <= (size_t)edges_per_face);

        if (edge_path.size() == (size_t)edges_per_face) {
            if (face_is_valid(mesh, edge_path)) {
                face_set.emplace(edge_path.cbegin(), edge_path.cend());
            }
        } else {
            for (int i = 0; i < vertex_edge_n; i++) {
                uint32_t edge_i = vertex_edge_indices[vertex_i * vertex_edge_n + i];
                if (!contains(edge_path, edge_i)) {
                    edge_path.insert(edge_i);
                    const Edge& edge = mesh.edges[edge_i];
                    uint32_t other_vertex_i = edge.v1 == vertex_i ? edge.v2 : edge.v1;
                    fill_face_set(other_vertex_i);
                    edge_path.erase(edge_i);
                }
            }
        }
    };

    for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
        assert(edge_path.size() == 0);
        fill_face_set(i);
    }

    mesh.faces = std::vector<Face>(face_set.cbegin(), face_set.cend());

    // Calculate cells

    // Calculate the number of adjacent faces per face
    int adjacent_faces_n = 0;
    {
        const Face& init_face = mesh.faces[0];
        for (uint32_t i = 1; i < mesh.faces.size(); i++) {
            const Face& other_face = mesh.faces[i];
            for (uint32_t edge_i : init_face) {
                if (contains(other_face, edge_i)) {
                    adjacent_faces_n++;
                    break;
                }
            }
        }
    }

    // Calculate all adjacent faces
    std::vector<uint32_t> adjacent_faces;
    adjacent_faces.reserve(mesh.faces.size() * adjacent_faces_n);

    for (uint32_t i = 0; i < mesh.faces.size(); i++) {
        const Face& current_face = mesh.faces[i];
        for (uint32_t other_i = 0; other_i < mesh.faces.size(); other_i++) {
            if (other_i != i) {
                const Face& other_face = mesh.faces[other_i];
                for (uint32_t edge_i : current_face) {
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
    std::unordered_set<uint32_t> face_path;

    // Recursive lambda definition
    std::function<void(uint32_t)> fill_cell_set;
    fill_cell_set = [&](uint32_t face_i) {
        assert(!contains(face_path, face_i));
        face_path.insert(face_i);
        assert(face_path.size() <= (size_t)faces_per_cell);

        if (face_path.size() == (size_t)faces_per_cell) {
            if (cell_is_valid(mesh, face_path)) {
                cell_set.emplace(face_path.cbegin(), face_path.cend());
            }
        } else {
            for (int adj_i = 0; adj_i < adjacent_faces_n; adj_i++) {
                uint32_t adj_face_i = adjacent_faces[face_i * adjacent_faces_n + adj_i];
                if (!contains(face_path, adj_face_i)) {
                    fill_cell_set(adj_face_i);
                }
            }
        }

        face_path.erase(face_i);
    };

    for (uint32_t i = 0; i < mesh.faces.size(); i++) {
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
