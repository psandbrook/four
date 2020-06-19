#pragma once

#include <four/app_state.hpp>
#include <four/mesh.hpp>
#include <four/utility.hpp>

#include <Eigen/Core>
#include <SDL.h>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

#include <vector>

namespace four {

struct Renderer {
private:
    SDL_Window* window;
    const AppState* state;

    // Variables initialized in the constructor
    // ----------------------------------------

    u32 shader_prog;
    u32 shader_prog_selected_cell;
    u32 vao;
    u32 vbo;
    u32 vao_selected_cell;
    u32 ebo_selected_cell;
    size_t ebo_selected_cell_size = 0;
    s32 vp_location;
    s32 selected_cell_vp_location;
    hmm_mat4 projection;

    // ----------------------------------------

    // Variables used for temporary storage in render()
    // ------------------------------------------------

    std::vector<hmm_vec3> projected_vertices;
    std::vector<f32> projected_vertices_f32;
    std::vector<u32> selected_cell_tri_faces;

    using VertexIMapping = boost::bimap<boost::bimaps::unordered_set_of<u32>, boost::bimaps::unordered_set_of<u32>>;
    VertexIMapping face2_vertex_i_mapping;

    std::vector<hmm_vec2> face2_vertices;
    std::vector<Edge> face2_edges;
    Eigen::MatrixX2d mesh_v;
    Eigen::MatrixX2i mesh_e;
    Eigen::MatrixX2d triangulate_out_v;
    Eigen::MatrixX3i triangulate_out_f;

    // ------------------------------------------------

public:
    Renderer(SDL_Window* window, const AppState* state);
    void render();

private:
    void triangulate(const Mesh4& mesh, u32 cell, std::vector<u32>& out_faces);
};
} // namespace four
