#include <four/render.hpp>

#include <four/math.hpp>

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <earcut.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <igl/copyleft/tetgen/tetrahedralize.h>
#include <loguru.hpp>

#include <functional>

namespace four {

namespace {

struct ConstFaceRef final : public std::reference_wrapper<const std::vector<Vec2>> {
    using value_type = type::value_type;

    explicit ConstFaceRef(const type& a) noexcept : std::reference_wrapper<type>(a) {}

    type::size_type size() const noexcept {
        return get().size();
    }

    bool empty() const noexcept {
        return get().empty();
    }

    type::const_reference operator[](type::size_type pos) const {
        return get()[pos];
    }
};
} // namespace

struct RenderFuncs::Impl {
    using VertexIMapping = boost::bimap<boost::bimaps::unordered_set_of<u32>, boost::bimaps::unordered_set_of<u32>>;
    VertexIMapping face2_vertex_i_mapping;

    std::vector<Vec2> face2_vertices;

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

void RenderFuncs::triangulate(const std::vector<Vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                              std::vector<u32>& out) {

    auto& st = *this->impl;

    using VertexIMapping = Impl::VertexIMapping;

    // Calculate normal vector

    u32 edge0_i = face[0];
    const Edge& edge0 = edges[edge0_i];
    u32 v0_i = edge0.v0;
    Vec3 v0 = vertices[v0_i];

    Vec3 normal;
    {
        Vec3 other_edge;
        for (u32 e_i : face) {
            if (e_i == edge0_i) {
                continue;
            }

            const Edge& e = edges[e_i];
            if (e.v0 == v0_i || e.v1 == v0_i) {
                u32 other_vi = e.v0 == v0_i ? e.v1 : e.v0;
                other_edge = vertices[other_vi] - v0;
                goto find_v0_edges_end;
            }
        }
        ABORT_F("Could not find normal vector");
    find_v0_edges_end:
        normal = glm::normalize(glm::cross(vertices[edge0.v1] - v0, other_edge));
    }

    if (float_eq(normal.x, 0.0) && float_eq(normal.y, 0.0) && float_eq(normal.z, 0.0)) {
        // Don't triangulate if the normal vector is the zero vector.
        return;
    }

    // Calculate transformation to 2D

    Vec3 up;
    if (float_eq(std::abs(normal.y), 1.0, 0.001)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    Mat4 to_2d_trans = glm::lookAt(v0, v0 + normal, up);

    st.face2_vertex_i_mapping.clear();
    st.face2_vertices.clear();

    const auto add_face2_vertex = [&](u32 v_i) -> bool {
        Vec3 v = vertices[v_i];
        for (const auto& entry : st.face2_vertex_i_mapping.left) {
            Vec3 u = vertices[entry.first];
            if (float_eq(v.x, u.x) && float_eq(v.y, u.y) && float_eq(v.z, u.z)) {
                // Don't triangulate if there are duplicate vertices.
                return false;
            }
        }

        st.face2_vertex_i_mapping.left.insert(VertexIMapping::left_value_type(v_i, (u32)st.face2_vertices.size()));

        Vec3 v_ = transform(to_2d_trans, v);
        DCHECK_F(float_eq(v_.z, 0.0));
        st.face2_vertices.push_back(Vec2(v_));
        return true;
    };

    if (!add_face2_vertex(v0_i)) {
        return;
    }

    u32 prev_edge_i = edge0_i;
    u32 current_v_i = edge0.v1;

    if (!add_face2_vertex(current_v_i)) {
        return;
    }

    while (true) {
        for (u32 e_i : face) {
            if (e_i != prev_edge_i) {
                const Edge& e = edges[e_i];
                if (e.v0 == current_v_i || e.v1 == current_v_i) {
                    u32 v_i = e.v0 == current_v_i ? e.v1 : e.v0;
                    if (v_i == v0_i) {
                        goto end_face2_loop;
                    }

                    if (!add_face2_vertex(v_i)) {
                        return;
                    }
                    prev_edge_i = e_i;
                    current_v_i = v_i;
                    goto find_next_v_i;
                }
            }
        }
        ABORT_F("Invalid face");
    find_next_v_i:;
    }
end_face2_loop:

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : st.face2_vertex_i_mapping.left) {
        if (entry.first != edge0.v0) {
            Vec3 v = vertices[entry.first];
            f64 x = glm::dot(v - v0, normal);

            // NOTE: Consider floating-point error. See
            // https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    const std::array<ConstFaceRef, 1> polygon = {ConstFaceRef(st.face2_vertices)};
    const std::vector<u32> result_indices = mapbox::earcut(polygon);

    for (u32 v_i : result_indices) {
        out.push_back(st.face2_vertex_i_mapping.right.at(v_i));
    }
}

bool RenderFuncs::tetrahedralize(const std::vector<Vec4>& vertices, const std::vector<Edge>& edges,
                                 const std::vector<Face>& faces, const Cell& cell, std::vector<Vec4>& out_vertices,
                                 std::vector<u32>& out_tets) {

    auto& s = *this->impl;

    // Calculate normal vector

    const Face& face0 = faces[cell[0]];
    u32 edge0_i = face0[0];
    const Edge& edge0 = edges[edge0_i];
    u32 v0_i = edge0.v0;
    Vec4 v0 = vertices[v0_i];

    Vec4 normal;
    {
        bool found_edge = false;
        u32 found_edge_i = (u32)-1;
        Vec4 other_edges[2];

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
        normal = glm::normalize(cross(vertices[edge0.v1] - v0, other_edges[0], other_edges[1]));
    }

    // Calculate transformation to 3D

    Vec4 up;
    Vec4 over;
    if (float_eq(std::abs(normal.y), 1.0, 0.001)) {
        up = {1, 0, 0, 0};
        over = {0, 0, 1, 0};
    } else if (float_eq(std::abs(normal.z), 1.0, 0.001)) {
        up = {0, 1, 0, 0};
        over = {1, 0, 0, 0};
    } else if (float_eq(sq(normal.y) + sq(normal.z), 1.0, 0.001)) {
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
        Vec4 v0_ = transform(to_3d_trans_inverse * to_3d_trans, v0);
        DCHECK_F(float_eq(v0.x, v0_.x) && float_eq(v0.y, v0_.y) && float_eq(v0.z, v0_.z) && float_eq(v0.w, v0_.w));
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
                    Vec4 v = vertices[v_i];
                    Vec4 v_ = transform(to_3d_trans, v);
                    DCHECK_F(float_eq(v_.w, 0.0));
                    Vec3 v_3 = Vec3(v_);
                    s.tet_mesh_v.push_back({v_3.x, v_3.y, v_3.z});
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

        DCHECK_EQ_F(f.size(), this_mesh_f.size());
        s.tet_mesh_f.push_back(std::move(this_mesh_f));
    }

    Mat4 temp_transform_inverse;
    {
        Vec3 centroid = Vec3(0, 0, 0);
        for (const auto& v : s.tet_mesh_v) {
            centroid += Vec3(v[0], v[1], v[2]);
        }

        centroid.x /= (f64)s.tet_mesh_v.size();
        centroid.y /= (f64)s.tet_mesh_v.size();
        centroid.z /= (f64)s.tet_mesh_v.size();

        Mat4 temp_translate = glm::translate(Mat4(1.0), -1.0 * centroid);

        for (auto& v : s.tet_mesh_v) {
            Vec3 v_ = transform(temp_translate, Vec3(v[0], v[1], v[2]));
            v[0] = v_.x;
            v[1] = v_.y;
            v[2] = v_.z;
        }

        temp_transform_inverse = glm::translate(Mat4(1.0), centroid);
    }

#ifdef FOUR_DEBUG
    // All vertices of the cell should be on the same hyperplane
    for (const auto& entry : s.cell3_vertex_i_mapping) {
        if (entry.first != edge0.v0) {
            Vec4 v = vertices[entry.first];
            f64 x = glm::dot(v - v0, normal);
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    s.tet_out_v.clear();
    s.tet_out_t.clear();
    s.tet_out_f.clear();
    int result = igl::copyleft::tetgen::tetrahedralize(s.tet_mesh_v, s.tet_mesh_f, "pYQ", s.tet_out_v, s.tet_out_t,
                                                       s.tet_out_f);
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
        DCHECK_EQ_F(v.size(), 3u);
        Vec3 v_ = Vec3(v[0], v[1], v[2]);
        v_ = transform(temp_transform_inverse, v_);
        out_vertices.push_back(transform(to_3d_trans_inverse, Vec4(v_, 0)));
    }

    for (const auto& tet : s.tet_out_t) {
        DCHECK_EQ_F(tet.size(), 4u);
        for (s32 i : tet) {
            out_tets.push_back(s.tet_out_vertex_i_mapping.at((u32)i));
        }
    }

    return true;
}
} // namespace four

namespace mapbox {
namespace util {

template <>
struct nth<0, four::Vec2> {
    static double get(const four::Vec2& v) {
        return v.x;
    }
};

template <>
struct nth<1, four::Vec2> {
    static double get(const four::Vec2& v) {
        return v.y;
    }
};
} // namespace util
} // namespace mapbox
