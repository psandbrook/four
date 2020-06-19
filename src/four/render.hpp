#pragma once

#include <four/app_state.hpp>
#include <four/triangulate.hpp>
#include <four/utility.hpp>

#include <glad/glad.h>

struct SDL_Window;

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
    void buffer_data_realloc(const void* data, size_t size);
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
    void buffer_data_realloc(const void* data, s32 n);
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
    AppState* state;

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

    std::vector<f32> mesh_colors;
    std::vector<hmm_vec3> projected_vertices;
    std::vector<f32> projected_vertices_f32;
    std::vector<u32> selected_cell_tri_faces;

    TriangulateFn triangulate;

    // ------------------------------------------------

public:
    Renderer(SDL_Window* window, AppState* state);
    void render();

private:
    size_t add_vbo(GLenum usage);
    void update_window_size();
    void update_mesh_buffers();
};
} // namespace four
