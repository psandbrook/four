#pragma once

#include <four/app_state.hpp>
#include <four/utility.hpp>

#include <glad/glad.h>

#include <memory>
#include <random>

struct SDL_Window;

namespace four {

struct ShaderProgram {
    u32 id;
    std::unordered_map<const char*, s32, CStrHash, CStrEquals> uniform_locations;

    ShaderProgram() {}
    ShaderProgram(u32 vertex_shader, u32 fragment_shader);

    void set_uniform_mat4(const char* name, const f32* data);
    void set_uniform_f32(const char* name, f32 value);
    s32 get_location(const char* name);
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

struct RenderFuncs {
private:
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    RenderFuncs();
    ~RenderFuncs();

    void triangulate(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                     std::vector<u32>& out);

    bool tetrahedralize(const std::vector<hmm_vec4>& vertices, const std::vector<Edge>& edges,
                        const std::vector<Face>& faces, const Cell& cell, std::vector<hmm_vec4>& out_vertices,
                        std::vector<u32>& out_tets);
};

// TODO: Add SRGB support
struct Renderer {
private:
    SDL_Window* window;
    AppState* state;

    ShaderProgram wireframe_shader_prog;
    ShaderProgram selected_cell_shader_prog;
    ShaderProgram cross_section_shader_prog;
    ShaderProgram xz_grid_shader_prog;

    std::vector<VertexBufferObject> vbos;

    VertexArrayObject wireframe;
    VertexArrayObject selected_cell;
    VertexArrayObject cross_section;
    VertexArrayObject xz_grid;

    hmm_mat4 projection;

    std::vector<hmm_vec4> tet_mesh_vertices;
    std::uniform_real_distribution<f32> color_dist;

    struct Tet {
        f32 color[3];
        u32 vertices[4];
    };
    std::vector<Tet> tet_mesh_tets;

    std::vector<f32> cross_vertices;
    std::vector<f32> cross_colors;
    std::vector<u32> cross_tris;

    // Temporary storage
    // ------------------------------------------------

    std::vector<hmm_vec4> projected_vertices;
    std::vector<hmm_vec3> projected_vertices3;
    std::vector<f32> projected_vertices_f32;
    std::vector<u32> selected_cell_tri_faces;

    std::vector<hmm_vec4> tet_mesh_vertices_world;
    std::vector<u32> out_tets;

    RenderFuncs render_funcs;

    // ------------------------------------------------

public:
    Renderer(SDL_Window* window, AppState* state);
    void render();

private:
    size_t add_vbo(GLenum usage);
    void update_window_size();
    void do_mesh_changed();
    void calculate_cross_section();
};
} // namespace four
