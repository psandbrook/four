#include <four/render.hpp>

#include <four/math.hpp>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <igl/copyleft/tetgen/tetrahedralize.h>
#include <igl/triangle/triangulate.h>
#include <loguru.hpp>

namespace four {

struct RenderFuncs::Impl {
    using VertexIMapping = boost::bimap<boost::bimaps::unordered_set_of<u32>, boost::bimaps::unordered_set_of<u32>>;
    VertexIMapping face2_vertex_i_mapping;

    std::vector<hmm_vec2> face2_vertices;
    std::vector<Edge> face2_edges;
    Eigen::MatrixX2d mesh_v;
    Eigen::MatrixX2i mesh_e;
    Eigen::MatrixX2d triangulate_out_v;
    Eigen::MatrixX3i triangulate_out_f;

    std::unordered_map<u32, u32> cell3_vertex_i_mapping;
    std::unordered_map<u32, u32> tet_out_vertex_i_mapping;
    std::vector<std::vector<f64>> tet_mesh_v;
    std::vector<std::vector<s32>> tet_mesh_f;
    std::vector<std::vector<f64>> tet_out_v;
    std::vector<std::vector<s32>> tet_out_t;
    std::vector<std::vector<s32>> tet_out_f;
};

RenderFuncs::RenderFuncs() : impl(std::make_unique<Impl>()) {}

RenderFuncs::~RenderFuncs() {}

void RenderFuncs::triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                              std::vector<u32>& out) {

    auto& s = *this->impl;

    using VertexIMapping = Impl::VertexIMapping;

    // Calculate normal vector

    const auto& edge0 = edges[face[0]];
    hmm_vec3 v0 = vertices[edge0.v0];
    hmm_vec3 l0 = v0 - vertices[edge0.v1];

    hmm_vec3 normal;

    // FIXME: Change this to search for edges of v0 (like tetrahedralize())
    for (size_t i = 0; i < face.size(); i++) {
        const auto& edge = edges[face[i]];
        hmm_vec3 l1 = vertices[edge.v0] - vertices[edge.v1];
        normal = HMM_Cross(l0, l1);
        if (float_eq(normal.X, 0.0) && float_eq(normal.Y, 0.0) && float_eq(normal.Z, 0.0)) {
            // `normal` is the zero vector

            // Fail if there are no more edges---this means the face has
            // no surface area
            CHECK_LT_F(i, face.size() - 1);
        } else {
            break;
        }
    }

    normal = HMM_Normalize(normal);

    // Calculate transformation to 2D

    hmm_vec3 up;
    if (float_eq(std::abs(normal.Y), 1.0, 0.001)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    hmm_mat4 to_2d_trans = HMM_LookAt(v0, v0 + normal, up);

    s.face2_vertex_i_mapping.clear();
    s.face2_vertices.clear();
    s.face2_edges.clear();

    for (u32 edge_i : face) {
        const auto& e = edges[edge_i];
        for (u32 v_i : e.vertices) {
            if (s.face2_vertex_i_mapping.left.find(v_i) == s.face2_vertex_i_mapping.left.end()) {
                s.face2_vertex_i_mapping.left.insert(
                        VertexIMapping::left_value_type(v_i, (u32)s.face2_vertices.size()));
                hmm_vec3 v_ = transform(to_2d_trans, vertices[v_i]);
                DCHECK_F(float_eq(v_.Z, 0.0));
                s.face2_vertices.push_back(vec2(v_));
            }
        }
        s.face2_edges.push_back(edge(s.face2_vertex_i_mapping.left.at(e.v0), s.face2_vertex_i_mapping.left.at(e.v1)));
    }

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : s.face2_vertex_i_mapping.left) {
        if (entry.first != edge0.v0) {
            hmm_vec3 v = vertices[entry.first];
            f64 x = HMM_Dot(v - v0, normal);

            // NOTE: Consider floating-point error. See
            // https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    // `igl::triangle::triangulate()` only accepts double precision
    // floating point values.
    s.mesh_v.resize((s64)s.face2_vertices.size(), Eigen::NoChange);
    for (size_t i = 0; i < s.face2_vertices.size(); i++) {
        hmm_vec2 v = s.face2_vertices[i];
        s.mesh_v((s64)i, 0) = v.X;
        s.mesh_v((s64)i, 1) = v.Y;
    }

    s.mesh_e.resize((s64)s.face2_edges.size(), Eigen::NoChange);
    for (size_t i = 0; i < s.face2_edges.size(); i++) {
        Edge e = s.face2_edges[i];
        s.mesh_e((s64)i, 0) = (s32)e.v0;
        s.mesh_e((s64)i, 1) = (s32)e.v1;
    }

    // TODO: Support triangulating faces with holes
    const Eigen::MatrixX2d h;

    s.triangulate_out_v.resize(0, Eigen::NoChange);
    s.triangulate_out_f.resize(0, Eigen::NoChange);
    igl::triangle::triangulate(s.mesh_v, s.mesh_e, h, "Q", s.triangulate_out_v, s.triangulate_out_f);

    DCHECK_EQ_F((size_t)s.triangulate_out_v.rows(), s.face2_vertices.size());

#ifdef FOUR_DEBUG
    for (s32 i = 0; i < s.triangulate_out_v.rows(); i++) {
        DCHECK_F(float_eq(s.triangulate_out_v(i, 0), s.face2_vertices[(size_t)i].X));
        DCHECK_F(float_eq(s.triangulate_out_v(i, 1), s.face2_vertices[(size_t)i].Y));
    }
#endif

    for (s32 i = 0; i < s.triangulate_out_f.rows(); i++) {
        for (s32 j = 0; j < 3; j++) {
            out.push_back(s.face2_vertex_i_mapping.right.at(s.triangulate_out_f(i, j)));
        }
    }
}

bool RenderFuncs::tetrahedralize(const std::vector<hmm_vec4>& vertices, const std::vector<Edge>& edges,
                                 const std::vector<Face>& faces, const Cell& cell, std::vector<hmm_vec4>& out_vertices,
                                 std::vector<u32>& out_tets) {

    auto& s = *this->impl;

    // Calculate normal vector

    const Face& face0 = faces[cell[0]];
    u32 edge0_i = face0[0];
    const Edge& edge0 = edges[edge0_i];
    u32 v0_i = edge0.v0;
    hmm_vec4 v0 = vertices[v0_i];

    hmm_vec4 normal;
    {
        bool found_edge = false;
        u32 found_edge_i = (u32)-1;
        hmm_vec4 other_edges[2];

        for (u32 f_i : cell) {
            const Face& f = faces[f_i];
            for (u32 e_i : f) {

                if (e_i == edge0_i || (found_edge && e_i == found_edge_i)) {
                    continue;
                }

                const Edge& e = edges[e_i];
                if (e.v0 == v0_i || e.v1 == v0_i) {
                    u32 other_vi = e.v0 == v0_i ? e.v1 : e.v0;
                    if (!found_edge) {
                        found_edge = true;
                        found_edge_i = e_i;
                        other_edges[0] = vertices[other_vi] - v0;
                    } else {
                        other_edges[1] = vertices[other_vi] - v0;
                        goto find_v0_edges_end;
                    }
                }
            }
        }
        ABORT_F("Could not find normal vector");
    find_v0_edges_end:
        normal = HMM_Normalize(cross(vertices[edge0.v1] - v0, other_edges[0], other_edges[1]));
    }

    // Calculate transformation to 3D

    hmm_vec4 up;
    hmm_vec4 over;
    if (float_eq(std::abs(normal.Y), 1.0, 0.001)) {
        up = {1, 0, 0, 0};
        over = {0, 0, 1, 0};
    } else if (float_eq(std::abs(normal.Z), 1.0, 0.001)) {
        up = {0, 1, 0, 0};
        over = {1, 0, 0, 0};
    } else if (float_eq(sq(normal.Y) + sq(normal.Z), 1.0, 0.001)) {
        up = {1, 0, 0, 0};
        over = {0, 0, 0, 1};
    } else {
        up = {0, 1, 0, 0};
        over = {0, 0, 1, 0};
    }

    Mat5 to_3d_trans = look_at(v0, v0 + normal, up, over);
    Mat5 to_3d_trans_inverse = look_at_inverse(v0, v0 + normal, up, over);

#ifdef FOUR_DEBUG
    {
        hmm_vec4 v0_ = transform(to_3d_trans_inverse * to_3d_trans, v0);
        DCHECK_F(float_eq(v0.X, v0_.X) && float_eq(v0.Y, v0_.Y) && float_eq(v0.Z, v0_.Z) && float_eq(v0.W, v0_.W));
    }
#endif

    s.cell3_vertex_i_mapping.clear();
    s.tet_mesh_v.clear();
    s.tet_mesh_f.clear();

    for (u32 f_i : cell) {
        const Face& f = faces[f_i];

        for (u32 e_i : f) {
            const Edge& e = edges[e_i];
            for (u32 v_i : e.vertices) {
                if (!has_key(s.cell3_vertex_i_mapping, v_i)) {
                    s.cell3_vertex_i_mapping.emplace(v_i, s.tet_mesh_v.size());
                    hmm_vec4 v = vertices[v_i];
                    hmm_vec4 v_ = transform(to_3d_trans, v);
                    DCHECK_F(float_eq(v_.W, 0.0));
                    hmm_vec3 v_3 = vec3(v_);
                    s.tet_mesh_v.push_back({v_3.X, v_3.Y, v_3.Z});
                }
            }
        }

        const Edge& e0 = edges[f[0]];
        u32 first_vi = e0.v0;
        std::vector<s32> this_mesh_f = {(s32)s.cell3_vertex_i_mapping.at(first_vi)};
        u32 prev_edge_i = f[0];
        u32 next_vi = e0.v1;

        while (next_vi != first_vi) {
            for (u32 e_i : f) {
                if (e_i != prev_edge_i) {
                    const Edge& e = edges[e_i];
                    if (e.v0 == next_vi || e.v1 == next_vi) {
                        this_mesh_f.push_back((s32)s.cell3_vertex_i_mapping.at(next_vi));
                        next_vi = e.v0 == next_vi ? e.v1 : e.v0;
                        prev_edge_i = e_i;

                        goto search_next_vi;
                    }
                }
            }
            // If the for-loop exits without finding `next_vi`, this is an
            // invalid face.
            ABORT_F("Invalid face");
        search_next_vi:;
        }

        CHECK_EQ_F(f.size(), this_mesh_f.size());
        s.tet_mesh_f.push_back(std::move(this_mesh_f));
    }

    hmm_mat4 temp_transform_inverse;
    {
        const auto& v0 = s.tet_mesh_v[(size_t)s.tet_mesh_f[0][0]];
        const auto& v1 = s.tet_mesh_v[(size_t)s.tet_mesh_f[0][1]];
        const auto& v2 = s.tet_mesh_v[(size_t)s.tet_mesh_f[0][2]];
        hmm_vec3 v0_ = HMM_Vec3(v0[0], v0[1], v0[2]);
        hmm_vec3 v1_ = HMM_Vec3(v1[0], v1[1], v1[2]);
        hmm_vec3 v2_ = HMM_Vec3(v2[0], v2[1], v2[2]);
        hmm_vec3 l1 = v1_ - v0_;
        hmm_vec3 n3d_normal = HMM_Normalize(HMM_Cross(l1, v2_ - v0_));

        hmm_mat4 temp_look_at = HMM_LookAt(v0_, v0_ + n3d_normal, l1);
        hmm_mat4 temp_look_at_inverse = look_at_inverse(v0_, v0_ + n3d_normal, l1);

        for (auto& v : s.tet_mesh_v) {
            hmm_vec3 v_ = transform(temp_look_at, HMM_Vec3(v[0], v[1], v[2]));
            v[0] = v_.X;
            v[1] = v_.Y;
            v[2] = v_.Z;
        }

        hmm_vec3 centroid = HMM_Vec3(0, 0, 0);
        for (const auto& v : s.tet_mesh_v) {
            centroid += HMM_Vec3(v[0], v[1], v[2]);
        }

        centroid.X /= (f64)s.tet_mesh_v.size();
        centroid.Y /= (f64)s.tet_mesh_v.size();
        centroid.Z /= (f64)s.tet_mesh_v.size();

        hmm_mat4 temp_translate = HMM_Translate(-1.0 * centroid);

        for (auto& v : s.tet_mesh_v) {
            hmm_vec3 v_ = transform(temp_translate, HMM_Vec3(v[0], v[1], v[2]));
            v[0] = v_.X;
            v[1] = v_.Y;
            v[2] = v_.Z;
        }

        temp_transform_inverse = temp_look_at_inverse * HMM_Translate(centroid);
    }

#ifdef FOUR_DEBUG
    // All vertices of the cell should be on the same hyperplane
    for (const auto& entry : s.cell3_vertex_i_mapping) {
        if (entry.first != edge0.v0) {
            hmm_vec4 v = vertices[entry.first];
            f64 x = HMM_Dot(v - v0, normal);
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    s.tet_out_v.clear();
    s.tet_out_t.clear();
    s.tet_out_f.clear();
    int result = igl::copyleft::tetgen::tetrahedralize(s.tet_mesh_v, s.tet_mesh_f, "pq1.2/18YQ", s.tet_out_v,
                                                       s.tet_out_t, s.tet_out_f);
    fflush(stdout);
    fflush(stderr);
    if (result != 0) {
        LOG_F(ERROR, "TetGen failed");
        return false;
    }

    s.tet_out_vertex_i_mapping.clear();
    for (size_t i = 0; i < s.tet_out_v.size(); i++) {
        s.tet_out_vertex_i_mapping.emplace(i, out_vertices.size());
        const auto& v = s.tet_out_v[i];
        CHECK_EQ_F(v.size(), 3u);
        hmm_vec3 v_ = HMM_Vec3(v[0], v[1], v[2]);
        v_ = transform(temp_transform_inverse, v_);
        out_vertices.push_back(transform(to_3d_trans_inverse, vec4(v_, 0)));
    }

    for (const auto& tet : s.tet_out_t) {
        CHECK_EQ_F(tet.size(), 4u);
        for (s32 i : tet) {
            out_tets.push_back(s.tet_out_vertex_i_mapping.at((u32)i));
        }
    }

    return true;
}
} // namespace four
