#include <four/render.hpp>

#include <four/math.hpp>

#include <earcut.hpp>
#include <loguru.hpp>

#include <functional>

namespace four {

namespace {

struct ConstFaceRef final : public std::reference_wrapper<const std::vector<glm::dvec2>> {
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
    std::unordered_map<u32, u32> face2_vertex_i_mapping_left;
    std::unordered_map<u32, u32> face2_vertex_i_mapping_right;
    std::vector<glm::dvec2> face2_vertices;
};

RenderFuncs::RenderFuncs() : impl(std::make_unique<Impl>()) {}

RenderFuncs::~RenderFuncs() {}

void RenderFuncs::triangulate(const std::vector<glm::dvec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                              std::vector<u32>& out) {

    auto& st = *this->impl;

    // Calculate normal vector

    u32 edge0_i = face[0];
    const Edge& edge0 = edges[edge0_i];
    u32 v0_i = edge0.v0;
    glm::dvec3 v0 = vertices[v0_i];
    glm::dvec3 edge0_vec = vertices[edge0.v1] - v0;

    glm::dvec3 normal;
    {
        glm::dvec3 other_edge_vec;
        for (u32 e_i : face) {
            if (e_i == edge0_i) {
                continue;
            }

            const Edge& e = edges[e_i];
            if (e.v0 == v0_i || e.v1 == v0_i) {
                u32 other_vi = e.v0 == v0_i ? e.v1 : e.v0;
                other_edge_vec = vertices[other_vi] - v0;
                goto find_v0_edges_end;
            }
        }
        ABORT_F("Could not find normal vector");

    find_v0_edges_end:
        if (float_eq(edge0_vec, other_edge_vec) || float_eq(edge0_vec, glm::dvec3(0.0))
            || float_eq(other_edge_vec, glm::dvec3(0.0))) {

            // Don't triangulate if the face is degenerate.
            return;
        }

        normal = glm::normalize(glm::cross(edge0_vec, other_edge_vec));
    }

    // Calculate transformation to 2D

    glm::dvec3 up;
    if (float_eq(std::abs(normal.y), 1.0, 0.001)) {
        up = {1, 0, 0};
    } else {
        up = {0, 1, 0};
    }

    glm::dmat4 to_2d_trans = glm::lookAt(v0, v0 + normal, up);

    st.face2_vertex_i_mapping_left.clear();
    st.face2_vertex_i_mapping_right.clear();
    st.face2_vertices.clear();

    const auto add_face2_vertex = [&](u32 v_i) -> bool {
        glm::dvec3 v = vertices[v_i];
        for (const auto& entry : st.face2_vertex_i_mapping_left) {
            glm::dvec3 u = vertices[entry.first];
            if (float_eq(v, u)) {
                // Don't triangulate if there are duplicate vertices.
                return false;
            }
        }

        st.face2_vertex_i_mapping_left.emplace(v_i, (u32)st.face2_vertices.size());
        st.face2_vertex_i_mapping_right.emplace((u32)st.face2_vertices.size(), v_i);

        glm::dvec3 v_ = transform(to_2d_trans, v);
        DCHECK_F(float_eq(v_.z, 0.0));
        st.face2_vertices.push_back(glm::dvec2(v_));
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

    DCHECK_EQ_F(st.face2_vertex_i_mapping_left.size(), st.face2_vertex_i_mapping_right.size());

#ifdef FOUR_DEBUG
    // All vertices should be coplanar
    for (const auto& entry : st.face2_vertex_i_mapping_left) {
        if (entry.first != edge0.v0) {
            glm::dvec3 v = vertices[entry.first];
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
        out.push_back(st.face2_vertex_i_mapping_right.at(v_i));
    }
}
} // namespace four

namespace mapbox {
namespace util {

template <>
struct nth<0, glm::dvec2> {
    static double get(const glm::dvec2& v) {
        return v.x;
    }
};

template <>
struct nth<1, glm::dvec2> {
    static double get(const glm::dvec2& v) {
        return v.y;
    }
};
} // namespace util
} // namespace mapbox
