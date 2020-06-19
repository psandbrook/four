#include <four/generate.hpp>

#include <four/utility.hpp>

#include <loguru.hpp>

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace four {

namespace {

const f64 golden_ratio = (1.0 + sqrt(5.0)) / 2.0;

// Heap's algorithm (see https://en.wikipedia.org/wiki/Heap's_algorithm)
void do_generate_permutations(s32 n, bool& even, Vec4& temp, std::unordered_set<Vec4>& out, bool only_even) {
    if (n == 1) {
        if (!only_even || even) {
            out.insert(temp);
        }
    } else {
        for (s32 i = 0; i < n - 1; i++) {
            do_generate_permutations(n - 1, even, temp, out, only_even);
            if (n % 2 == 0) {
                std::swap(temp[i], temp[n - 1]);
            } else {
                std::swap(temp[0], temp[n - 1]);
            }
            even = !even;
        }
        do_generate_permutations(n - 1, even, temp, out, only_even);
    }
}

inline void generate_permutations(Vec4 in, std::unordered_set<Vec4>& out, bool only_even = false) {
    bool even = true;
    do_generate_permutations(4, even, in, out, only_even);
}

// clang-format off
const Vec4 n5cell_vertices[5] = {
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
const s32 n5cell_n_cells = 5;

// clang-format off
const Vec4 tesseract_vertices[16] = {
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
const s32 tesseract_n_cells = 8;

// clang-format off
const Vec4 n16cell_vertices[8] = {
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
const s32 n16cell_n_cells = 16;

// clang-format off
const Vec4 n24cell_vertices[24] = {
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
const s32 n24cell_n_cells = 24;

// clang-format off
const Vec4 n120cell_base_vertices[] = {
    {0, 0, 2, 2},
    {1, 1, 1, sqrt(5.0)},
    {pow(golden_ratio, -2.0), golden_ratio, golden_ratio, golden_ratio},
    {pow(golden_ratio, -1.0), pow(golden_ratio, -1.0), pow(golden_ratio, -1.0), pow(golden_ratio, 2.0)},
};

const Vec4 n120cell_base_vertices_even[] = {
    {0, pow(golden_ratio, -2.0), 1, pow(golden_ratio, 2.0)},
    {0, pow(golden_ratio, -1.0), golden_ratio, sqrt(5.0)},
    {pow(golden_ratio, -1.0), 1, golden_ratio, 2},
};
// clang-format on

std::vector<Vec4> generate_120cell_vertices() {
    std::unordered_set<Vec4> permutations;

    for (const auto& v : n120cell_base_vertices) {
        generate_permutations(v, permutations);
    }

    for (const auto& v : n120cell_base_vertices_even) {
        generate_permutations(v, permutations, true);
    }

    std::vector<Vec4> vertices;
    std::unordered_set<Vec4> seen;
    for (Vec4 v : permutations) {
        for (s32 a = 0; a < 2; a++) {
            for (s32 b = 0; b < 2; b++) {
                for (s32 c = 0; c < 2; c++) {
                    for (s32 d = 0; d < 2; d++) {
                        if (seen.insert(v).second) {
                            vertices.push_back(v);
                        }
                        v[3] = -v[3];
                    }
                    v[2] = -v[2];
                }
                v[1] = -v[1];
            }
            v[0] = -v[0];
        }
    }

    return vertices;
}

const f64 n120cell_edge_length = 2.0 / pow(golden_ratio, 2.0);
const s32 n120cell_edges_per_face = 5;
const s32 n120cell_faces_per_cell = 12;
const s32 n120cell_n_cells = 120;

const Vec4 n600cell_base_vertex0 = {0.5, 0.5, 0.5, 0.5};
const Vec4 n600cell_base_vertex1 = {0, 0, 0, 1};
const Vec4 n600cell_base_vertex2 = {golden_ratio / 2.0, 0.5, 1.0 / (2 * golden_ratio), 0};

std::vector<Vec4> generate_600cell_vertices() {
    std::unordered_set<Vec4> permutations;

    permutations.insert(n600cell_base_vertex0);
    generate_permutations(n600cell_base_vertex1, permutations);
    generate_permutations(n600cell_base_vertex2, permutations, true);

    std::vector<Vec4> vertices;
    std::unordered_set<Vec4> seen;
    for (Vec4 v : permutations) {
        for (s32 a = 0; a < 2; a++) {
            for (s32 b = 0; b < 2; b++) {
                for (s32 c = 0; c < 2; c++) {
                    for (s32 d = 0; d < 2; d++) {
                        if (seen.insert(v).second) {
                            vertices.push_back(v);
                        }
                        v[3] = -v[3];
                    }
                    v[2] = -v[2];
                }
                v[1] = -v[1];
            }
            v[0] = -v[0];
        }
    }

    return vertices;
}

const f64 n600cell_edge_length = 1.0 / golden_ratio;
const s32 n600cell_edges_per_face = 3;
const s32 n600cell_faces_per_cell = 4;
const s32 n600cell_n_cells = 600;

Mesh4 generate_mesh4(const Vec4* vertices, const u32 n_vertices, const f64 edge_length, const s32 edges_per_face,
                     const s32 faces_per_cell, const s32 n_cells) {
    Mesh4 mesh;

    mesh.vertices = std::vector<Vec4>(vertices, vertices + n_vertices);
    LOG_F(INFO, "%lu vertices", mesh.vertices.size());

    // Calculate edges
    {
        std::unordered_set<Edge> edge_set;

        for (u32 i = 0; i < mesh.vertices.size(); i++) {
            for (u32 j = 0; j < mesh.vertices.size(); j++) {
                if (j != i && float_eq(glm::length(mesh.vertices[j] - mesh.vertices[i]), edge_length)) {
                    edge_set.insert(Edge(i, j));
                }
            }
        }

        mesh.edges = std::vector<Edge>(edge_set.cbegin(), edge_set.cend());
        LOG_F(INFO, "Found %lu edges", mesh.edges.size());
    }

    // Calculate faces
    {
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

        std::unordered_map<u32, s32> vertices_count;
        const auto face_is_valid = [&](const std::unordered_set<u32>& face) -> bool {
            vertices_count.clear();

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
        };

        std::unordered_set<Face, FaceHash, FaceEquals> face_set;
        std::unordered_set<u32> edge_path;

        // Recursive lambda definition
        std::function<void(u32)> fill_face_set;
        fill_face_set = [&](u32 vertex_i) {
            assert(edge_path.size() <= (size_t)edges_per_face);

            if (edge_path.size() == (size_t)edges_per_face) {
                if (face_is_valid(edge_path)) {
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
        LOG_F(INFO, "Found %lu faces", mesh.faces.size());
    }

    // Calculate cells
    {
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

        // Collect vertex indices of all faces
        std::vector<u32> face_vertex_indices;
        const s32 vertices_per_face = edges_per_face;
        face_vertex_indices.reserve(mesh.faces.size() * (size_t)vertices_per_face);

        for (const auto& face : mesh.faces) {
            std::unordered_set<u32> seen_vertex_indices;
            for (u32 edge_i : face) {
                const Edge& edge = mesh.edges[edge_i];
                if (seen_vertex_indices.insert(edge.v0).second) {
                    face_vertex_indices.push_back(edge.v0);
                }
                if (seen_vertex_indices.insert(edge.v1).second) {
                    face_vertex_indices.push_back(edge.v1);
                }
            }
        }

        DCHECK_EQ_F(face_vertex_indices.size(), mesh.faces.size() * (size_t)vertices_per_face);

        // Returns true if the given faces share a vertex.
        const auto share_vertex = [&](u32 face_i1, u32 face_i2) -> bool {
            for (u32 vii1 = 0; vii1 < (u32)vertices_per_face; vii1++) {
                u32 vi1 = face_vertex_indices[face_i1 * (u32)vertices_per_face + vii1];

                for (u32 vii2 = 0; vii2 < (u32)vertices_per_face; vii2++) {
                    u32 vi2 = face_vertex_indices[face_i2 * (u32)vertices_per_face + vii2];

                    if (vi2 == vi1) {
                        return true;
                    }
                }
            }

            return false;
        };

        std::mutex cell_set_mutex;
        std::unordered_set<Cell, CellHash, CellEquals> cell_set;

        u32 n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) {
            n_threads = 1;
        } else if (n_threads > mesh.faces.size()) {
            n_threads = (u32)mesh.faces.size();
        }

        u32 search_n = (u32)mesh.faces.size() / n_threads;
        std::vector<std::thread> threads;

        LOG_F(INFO, "Starting %u threads for cell search", n_threads);
        for (u32 i = 0; i < n_threads; i++) {

            u32 search_start = search_n * i;
            u32 search_end = i == n_threads - 1 ? (u32)mesh.faces.size() : search_start + search_n;

            auto t = std::thread([&, i, search_start, search_end]() {
                loguru::set_thread_name(loguru::textprintf("cell_search%u", i).c_str());

                std::unordered_map<u32, s32> edges_count;
                const auto cell_is_valid = [&](const std::unordered_set<u32>& cell) -> bool {
                    edges_count.clear();

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
                };

                std::unordered_set<u32> face_path;

                // Recursive lambda definition
                std::function<void(s64, s64, u32)> fill_cell_set;
                fill_cell_set = [&](s64 parent_face_i, s64 gparent_face_i, u32 face_i) {
                    DCHECK_F(!contains(face_path, face_i));
                    face_path.insert(face_i);
                    DCHECK_F(face_path.size() <= (size_t)faces_per_cell);

                    // TODO: Check if current face_path is convex

                    if (face_path.size() == (size_t)faces_per_cell) {
                        if (cell_is_valid(face_path)) {

                            bool success;
                            size_t cell_set_size;
                            {
                                auto lock = std::scoped_lock(cell_set_mutex);
                                success = cell_set.emplace(face_path.cbegin(), face_path.cend()).second;
                                if (success) {
                                    cell_set_size = cell_set.size();
                                }
                            }

                            if (success) {
                                LOG_F(INFO, "Current cells found: %lu", cell_set_size);
                            }
                        }
                    } else {
                        for (u32 adj_i = 0; adj_i < adjacent_faces_n; adj_i++) {
                            u32 adj_face_i = adjacent_faces[face_i * adjacent_faces_n + adj_i];

                            if (!contains(face_path, adj_face_i)
                                && (parent_face_i == -1 || share_vertex(adj_face_i, (u32)parent_face_i)
                                    || (gparent_face_i != -1 && share_vertex(adj_face_i, (u32)gparent_face_i)))) {

                                fill_cell_set(face_i, parent_face_i, adj_face_i);
                            }
                        }
                    }

                    const size_t result = face_path.erase(face_i);
                    DCHECK_EQ_F(result, 1u);
                };

                for (u32 i = search_start; i < search_end; i++) {
                    LOG_F(INFO, "Searching for cells at face %i ...", i);
                    face_path.clear();
                    fill_cell_set(-1, -1, i);

                    size_t cell_set_size;
                    {
                        auto lock = std::scoped_lock(cell_set_mutex);
                        cell_set_size = cell_set.size();
                    }

                    if (cell_set_size == (size_t)n_cells) {
                        break;
                    }
                }
            });

            threads.push_back(std::move(t));
        }

        for (auto& t : threads) {
            t.join();
        }

        mesh.cells = std::vector<Cell>(cell_set.cbegin(), cell_set.cend());
        LOG_F(INFO, "Total cells found: %lu", mesh.cells.size());
    }

    return mesh;
}
} // namespace

Mesh4 generate_5cell() {
    return generate_mesh4(n5cell_vertices, ARRAY_SIZE(n5cell_vertices), n5cell_edge_length, n5cell_edges_per_face,
                          n5cell_faces_per_cell, n5cell_n_cells);
}

Mesh4 generate_tesseract() {
    return generate_mesh4(tesseract_vertices, ARRAY_SIZE(tesseract_vertices), tesseract_edge_length,
                          tesseract_edges_per_face, tesseract_faces_per_cell, tesseract_n_cells);
}

Mesh4 generate_16cell() {
    return generate_mesh4(n16cell_vertices, ARRAY_SIZE(n16cell_vertices), n16cell_edge_length, n16cell_edges_per_face,
                          n16cell_faces_per_cell, n16cell_n_cells);
}

Mesh4 generate_24cell() {
    return generate_mesh4(n24cell_vertices, ARRAY_SIZE(n24cell_vertices), n24cell_edge_length, n24cell_edges_per_face,
                          n24cell_faces_per_cell, n24cell_n_cells);
}

Mesh4 generate_120cell() {
    const std::vector<Vec4> n120cell_vertices = generate_120cell_vertices();
    return generate_mesh4(n120cell_vertices.data(), (u32)n120cell_vertices.size(), n120cell_edge_length,
                          n120cell_edges_per_face, n120cell_faces_per_cell, n120cell_n_cells);
}

Mesh4 generate_600cell() {
    const std::vector<Vec4> n600cell_vertices = generate_600cell_vertices();
    return generate_mesh4(n600cell_vertices.data(), (u32)n600cell_vertices.size(), n600cell_edge_length,
                          n600cell_edges_per_face, n600cell_faces_per_cell, n600cell_n_cells);
}
} // namespace four
