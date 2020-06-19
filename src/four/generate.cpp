#include <four/generate.hpp>

#include <four/utility.hpp>

#include <HandmadeMath.h>

#include <assert.h>
#include <stdint.h>

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace four {

namespace {

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

std::unordered_set<Face, FaceHash> get_edge_paths(const Mesh4& mesh, uint32_t vertex, int n, bool skip_edge,
                                                  uint32_t skip_edge_i) {
    assert(n >= 1);
    if (n == 1) {
        std::unordered_set<Face, FaceHash> output;

        for (uint32_t i = 0; i < mesh.edges.size(); i++) {
            if (!skip_edge || i != skip_edge_i) {
                const Edge& edge = mesh.edges[i];
                if (vertex == edge.v1 || vertex == edge.v2) {
                    std::unordered_set<uint32_t> path;
                    path.insert(i);
                    output.insert(std::move(path));
                }
            }
        }

        return output;

    } else {
        std::unordered_set<Face, FaceHash> output;

        for (uint32_t i = 0; i < mesh.edges.size(); i++) {
            if (!skip_edge || i != skip_edge_i) {
                const Edge& edge = mesh.edges[i];
                if (vertex == edge.v1 || vertex == edge.v2) {
                    uint32_t other = vertex == edge.v1 ? edge.v2 : edge.v1;
                    auto rec_paths = get_edge_paths(mesh, other, n - 1, true, i);
                    for (const auto& path : rec_paths) {
                        std::unordered_set<uint32_t> path_p(path);
                        path_p.insert(i);
                        output.insert(std::move(path_p));
                    }
                }
            }
        }

        return output;
    }
}

std::unordered_set<Cell, CellHash> get_face_sets(const Mesh4& mesh, uint32_t face_i, int n, bool skip_face,
                                                 uint32_t skip_face_i) {
    assert(n >= 1);
    if (n == 1) {
        std::unordered_set<Cell, CellHash> output;

        std::unordered_set<uint32_t> set;
        set.insert(face_i);
        output.insert(std::move(set));

        return output;

    } else {
        std::unordered_set<Cell, CellHash> output;
        const Face& current_face = mesh.faces[face_i];

        // Find faces that share an edge with the current face
        for (uint32_t i = 0; i < mesh.faces.size(); i++) {
            if (i != face_i && (!skip_face || i != skip_face_i)) {
                const Face& other_face = mesh.faces[i];

                for (uint32_t edge_i : current_face) {
                    if (other_face.find(edge_i) != other_face.end()) {
                        // Found a matching face
                        auto rec_face_sets = get_face_sets(mesh, i, n - 1, true, face_i);
                        for (const auto& set : rec_face_sets) {
                            std::unordered_set<uint32_t> set_p(set);
                            set_p.insert(face_i);
                            output.insert(std::move(set_p));
                        }

                        break;
                    }
                }
            }
        }

        return output;
    }
}

bool face_is_valid(const Mesh4& mesh, const Face& face) {
    std::unordered_map<uint32_t, int> vertices_count;

    for (uint32_t edge_i : face) {
        const Edge& edge = mesh.edges[edge_i];
        uint32_t vert_indices[] = {edge.v1, edge.v2};
        for (uint32_t vert_i : vert_indices) {
            if (vertices_count.find(vert_i) == vertices_count.end()) {
                vertices_count.insert({vert_i, 1});
            } else {
                vertices_count[vert_i] = vertices_count[vert_i] + 1;
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

bool cell_is_valid(const Mesh4& mesh, const Cell& cell) {
    std::unordered_map<uint32_t, int> edges_count;
    for (uint32_t face_i : cell) {
        const Face& face = mesh.faces[face_i];
        for (uint32_t edge_i : face) {
            if (edges_count.find(edge_i) == edges_count.end()) {
                edges_count.insert({edge_i, 1});
            } else {
                edges_count[edge_i] = edges_count[edge_i] + 1;
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
            if (j != i
                && float_eq(HMM_LengthSquared(mesh.vertices[j] - mesh.vertices[i]), edge_length_sq,
                            flt_default_epsilon)) {
                Edge e = {.v1 = i, .v2 = j};
                edge_set.insert(e);
            }
        }
    }

    mesh.edges = std::vector<Edge>(edge_set.cbegin(), edge_set.cend());

    // Calculate faces

    std::unordered_set<Face, FaceHash> face_set;

    for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
        auto new_paths = get_edge_paths(mesh, i, edges_per_face, false, 0);
        for (const auto& p : new_paths) {
            if (face_is_valid(mesh, p)) {
                face_set.insert(p);
            }
        }
    }

    mesh.faces = std::vector<Face>(face_set.cbegin(), face_set.cend());

    // Calculate cells

    std::unordered_set<Cell, CellHash> cell_set;

#if 1
    {
        std::vector<uint32_t> faces;
        faces.reserve(mesh.faces.size());
        for (uint32_t i = 0; i < mesh.faces.size(); i++) {
            faces.push_back(i);
        }

        std::vector<uint32_t> tmp((size_t)faces_per_cell);
        assert(tmp.size() == (size_t)faces_per_cell);

        std::function<void(int, int)> fill_cell_set;
        fill_cell_set = [&](int len, int start_pos) {
            if (len == 0) {
                std::unordered_set<uint32_t> cell{tmp.cbegin(), tmp.cend()};
                if (cell_is_valid(mesh, cell)) {
                    cell_set.insert(std::move(cell));
                }
            } else {
                for (int i = start_pos; i <= (int)faces.size() - len; i++) {
                    tmp[tmp.size() - len] = faces[i];
                    fill_cell_set(len - 1, i + 1);
                }
            }
        };

        fill_cell_set(faces_per_cell, 0);
    }

#else
    for (uint32_t i = 0; i < mesh.faces.size(); i++) {
        auto new_face_sets = get_face_sets(mesh, i, faces_per_cell, false, 0);
        for (const auto& set : new_face_sets) {
            if (cell_is_valid(mesh, set)) {
                cell_set.insert(set);
            }
        }
    }
#endif

    mesh.cells = std::vector<Cell>(cell_set.cbegin(), cell_set.cend());

    return mesh;
}
} // namespace

Mesh4 generate_tesseract() {
    return generate_mesh4(tesseract_vertices, ARRAY_SIZE(tesseract_vertices), tesseract_edge_length,
                          tesseract_edges_per_face, tesseract_faces_per_cell);
}
} // namespace four
