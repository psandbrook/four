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

struct ShaderProgram {
    u32 id;
    std::unordered_map<const char*, s32, CStrHash, CStrEquals> uniform_locations;

    ShaderProgram() {}
    ShaderProgram(const char* vert_path, const char* frag_path);

    void set_uniform_mat4(const char* name, const f32* data);
};

struct VertexSpec {
    u32 index;
    s32 size;
    GLenum type;
    GLsizei stride;
    ptrdiff_t offset;
};

struct GlBuffer {
    u32 id;
    GLenum type;
    GLenum usage;
    size_t size = 0;

    GlBuffer() {}
    GlBuffer(GLenum type, GLenum usage);

    void buffer_data(const void* data, size_t size);
};

using VertexBufferObject = GlBuffer;

inline VertexBufferObject new_vertex_buffer_object(GLenum usage) {
    GlBuffer buf(GL_ARRAY_BUFFER, usage);
    return buf;
}

struct ElementBufferObject {
    GlBuffer buf;
    GLenum primitive;
    s32 primitive_count = 0;

    ElementBufferObject() {}
    ElementBufferObject(GLenum usage, GLenum primitive) : buf(GL_ELEMENT_ARRAY_BUFFER, usage), primitive(primitive) {}

    void buffer_data(const void* data, s32 n);
};

struct VertexArrayObject {
    u32 id;
    ShaderProgram* shader_program;
    std::vector<VertexBufferObject*> vbos; // Vertex buffer objects can be shared among many VAOs
    ElementBufferObject ebo;

    VertexArrayObject() {}
    VertexArrayObject(ShaderProgram* shader_program, Slice<VertexBufferObject*> vbos, Slice<VertexSpec> specs,
                      ElementBufferObject ebo);

    void draw();
};

struct Renderer {
private:
    SDL_Window* window;
    const AppState* state;

    // Variables initialized in the constructor
    // ----------------------------------------

    ShaderProgram shader_program;

    std::vector<VertexBufferObject> vbos;

    VertexArrayObject wireframe;
    VertexArrayObject selected_cell;
    VertexArrayObject xz_grid;

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
    void triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                     std::vector<u32>& out);
};
} // namespace four
