#pragma once

#include <four/app_state.hpp>
#include <four/mesh.hpp>
#include <four/utility.hpp>

#include <Eigen/Core>
#include <SDL.h>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <glad/glad.h>
#include <loguru.hpp>

#include <unordered_map>
#include <vector>

namespace four {

struct VertexSpec {
    u32 index;
    s32 size;
    GLenum type;
    GLsizei stride;
    ptrdiff_t offset;
};

class GlBuffer {
private:
    friend class VertexArrayObject;

    u32 id;
    GLenum type;
    GLenum usage;
    size_t size = 0;

public:
    GlBuffer() {}
    GlBuffer(GLenum type, GLenum usage);

    GlBuffer(const GlBuffer&) = delete;
    GlBuffer& operator=(const GlBuffer&) = delete;

    GlBuffer(GlBuffer&&) = default;
    GlBuffer& operator=(GlBuffer&&) = default;

    ~GlBuffer() {}

    void buffer_data(const void* data, size_t size);
};

struct ElementBufferObject {
private:
    friend class VertexArrayObject;

    GlBuffer buf;
    GLenum primitive;
    s32 primitive_count = 0;

public:
    ElementBufferObject() {}
    ElementBufferObject(GLenum usage, GLenum primitive);

    void buffer_data(const void* data, s32 n);
};

class VertexArrayObject {
public:
    GlBuffer* vbo; // A vertex buffer object can be shared among many VAOs
    ElementBufferObject ebo;

private:
    u32 id;
    u32 shader_program;
    std::unordered_map<const char*, s32, CStrHash, CStrEquals> uniform_locations;

public:
    VertexArrayObject() {}
    VertexArrayObject(u32 shader_program, GlBuffer* vbo, VertexSpec spec, ElementBufferObject ebo);

    VertexArrayObject(const VertexArrayObject&) = delete;
    VertexArrayObject& operator=(const VertexArrayObject&) = delete;

    VertexArrayObject(VertexArrayObject&&) = default;
    VertexArrayObject& operator=(VertexArrayObject&&) = default;

    ~VertexArrayObject() {}

    void draw();
    void set_uniform_mat4(const char* name, const f32* data);
};

struct Renderer {
private:
    SDL_Window* window;
    const AppState* state;

    // Variables initialized in the constructor
    // ----------------------------------------

    GlBuffer vertices;
    VertexArrayObject wireframe;
    VertexArrayObject selected_cell;
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
    void triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges,
                     const std::vector<u32>& face, std::vector<u32>& out);
};
} // namespace four
