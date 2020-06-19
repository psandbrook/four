#include <four/render.hpp>

#include <four/math.hpp>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <igl/triangle/triangulate.h>
#include <loguru.hpp>

namespace four {

struct Renderer::TriangulateFn::Impl {
    using VertexIMapping = boost::bimap<boost::bimaps::unordered_set_of<u32>, boost::bimaps::unordered_set_of<u32>>;
    VertexIMapping face2_vertex_i_mapping;

    std::vector<hmm_vec2> face2_vertices;
    std::vector<Edge> face2_edges;
    Eigen::MatrixX2d mesh_v;
    Eigen::MatrixX2i mesh_e;
    Eigen::MatrixX2d triangulate_out_v;
    Eigen::MatrixX3i triangulate_out_f;
};

Renderer::TriangulateFn::TriangulateFn() : impl(std::make_unique<Impl>()) {}

Renderer::TriangulateFn::~TriangulateFn() {}

void Renderer::TriangulateFn::operator()(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges,
                                         const Face& face, std::vector<u32>& out) {

    auto& s = *this->impl;

    using VertexIMapping = Impl::VertexIMapping;
    const f64 epsilon = 0.00000000000001;

    // Calculate normal vector

    const auto& edge0 = edges[face[0]];
    hmm_vec3 v0 = vertices[edge0.v0];
    hmm_vec3 l0 = v0 - vertices[edge0.v1];

    hmm_vec3 normal;
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
                DCHECK_F(float_eq(v_.Z, 0.0, epsilon));
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
            DCHECK_F(float_eq(x, 0.0, epsilon));
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
} // namespace four
