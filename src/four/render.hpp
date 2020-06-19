#pragma once

#include <four/app_state.hpp>
#include <four/math.hpp>
#include <four/utility.hpp>

#include <glad/glad.h>

#include <initializer_list>
#include <random>

struct SDL_Window;

namespace four {

struct GlBuffer {
public:
    u32 id;
    GLenum type;
    GLenum usage;
    size_t size = 0;

protected:
    GlBuffer() = default;
    GlBuffer(GLenum type, GLenum usage);

public:
    void buffer_data(const void* data, size_t size);
    void buffer_data_realloc(const void* data, size_t size);
};

struct VertexBufferObject : public GlBuffer {
    VertexBufferObject() = default;
    explicit VertexBufferObject(GLenum usage) : GlBuffer(GL_ARRAY_BUFFER, usage) {}
};

struct ElementBufferObject : public GlBuffer {
    GLenum primitive;
    s32 primitive_count = 0;

    ElementBufferObject() = default;
    ElementBufferObject(GLenum usage, GLenum primitive)
            : GlBuffer(GL_ELEMENT_ARRAY_BUFFER, usage), primitive(primitive) {}

    void buffer_elements(const void* data, s32 n);
    void buffer_elements_realloc(const void* data, s32 n);
};

struct UniformBufferObject : public GlBuffer {
    const char* name;
    u32 binding;

    UniformBufferObject() = default;
    UniformBufferObject(const char* name, u32 binding, GLenum usage);
};

struct Framebuffer {
    u32 id = 0;
    u32 width, height;

    CXX_EXTENSION union {
        struct {
            u32 color_rbo;
            u32 depth_rbo;
        };
        u32 rbos[2] = {};
    };

    Framebuffer() = default;
    Framebuffer(u32 width, u32 height);

    void bind();
    void destroy();
};

struct ShaderProgram {
    u32 id;
    std::unordered_map<const char*, s32, CStrHash, CStrEquals> uniform_locations;

    ShaderProgram() = default;
    ShaderProgram(u32 vertex_shader, std::initializer_list<u32> fragment_shaders);

    void set_uniform_f32(const char* name, f32 value);
    void set_uniform_bool(const char* name, bool value);
    void set_uniform_mat4(const char* name, const f32* data);
    void set_uniform_vec3(const char* name, const f32* data);

    s32 get_location(const char* name);

    void bind_uniform_block(const UniformBufferObject& ubo);
};

struct VertexSpec {
    u32 index;
    s32 size;
    GLenum type;
    GLsizei stride;
    ptrdiff_t offset;
};

struct VertexArrayObject {
    u32 id;
    ShaderProgram* shader_program;
    std::vector<u32> vbos; // Vertex buffer objects can be shared among many VAOs
    ElementBufferObject ebo;

    VertexArrayObject() = default;
    VertexArrayObject(ShaderProgram* shader_program, std::initializer_list<u32> vbos,
                      std::initializer_list<VertexSpec> specs, ElementBufferObject ebo);

    VertexBufferObject& get_vbo(size_t index);
    void draw();
};

struct Renderer {
public:
    SDL_Window* window;
    AppState* state;

    u32 vis_width_screen;

    UniformBufferObject view_projection_ubo;

    Framebuffer combined_buffer;
    Framebuffer projection_buffer;

    ShaderProgram n4d_shader_prog;
    ShaderProgram cross_section_shader_prog;
    ShaderProgram xz_grid_shader_prog;
    ShaderProgram divider_bar_shader_prog;

    std::vector<VertexBufferObject> vbos;

    VertexArrayObject wireframe;
    VertexArrayObject selected_cell;
    VertexArrayObject cross_section;
    VertexArrayObject xz_grid;
    VertexArrayObject divider_bar;

    std::uniform_real_distribution<f32> color_dist;
    std::vector<glm::vec3> tet_colors;

private:
    // Temporary storage
    // ------------------------------------------------

    std::unordered_map<u32, u32> face2_vertex_i_mapping_left;
    std::unordered_map<u32, u32> face2_vertex_i_mapping_right;
    std::vector<glm::dvec2> face2_vertices;

    std::vector<glm::dvec4> projected_vertices;
    std::vector<glm::dvec3> projected_vertices3;
    std::vector<f32> projected_vertices_f32;
    std::vector<u32> selected_cell_tri_faces;

    std::vector<glm::dvec4> tet_mesh_vertices_world;
    std::vector<f32> cross_vertices;
    std::vector<f32> cross_colors;
    std::vector<u32> cross_tris;

    // ------------------------------------------------

public:
    Renderer(SDL_Window* window, AppState* state);
    void render();

private:
    u32 add_vbo(GLenum usage);
    void do_mesh_changed();
    void do_window_size_changed();
    void triangulate(const std::vector<glm::dvec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                     std::vector<u32>& out);

    void calculate_cross_section();
};
} // namespace four
