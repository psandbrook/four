#include <four/mesh.hpp>

#include <loguru.hpp>
#include <tetgen.h>
#include <tinyxml2.h>

#include <algorithm>
#include <string.h>
#include <string>
#include <utility>

namespace txml = tinyxml2;

namespace four {

namespace {

void tetrahedralize_polyhedron(const std::vector<glm::dvec3>& vertices, const std::vector<std::vector<u32>>& faces,
                               std::vector<glm::dvec3>& out_vertices, std::vector<u32>& out_tets) {

    tetgenio tetgen_in;
    tetgen_in.numberofpoints = (int)vertices.size();
    tetgen_in.pointlist = new f64[vertices.size() * 3]{};
    for (size_t i = 0; i < vertices.size(); i++) {
        for (s32 j = 0; j < 3; j++) {
            tetgen_in.pointlist[i * 3 + (size_t)j] = vertices[i][j];
        }
    }

    tetgen_in.numberoffacets = (int)faces.size();
    tetgen_in.facetlist = new tetgenio::facet[faces.size()]{};
    for (size_t i = 0; i < faces.size(); i++) {
        const auto& face = faces[i];

        tetgenio::facet& facet = tetgen_in.facetlist[i];
        facet.numberofpolygons = 1;
        facet.polygonlist = new tetgenio::polygon[1]{};

        tetgenio::polygon& polygon = facet.polygonlist[0];
        polygon.numberofvertices = (int)face.size();
        polygon.vertexlist = new int[face.size()]{};
        for (size_t j = 0; j < face.size(); j++) {
            polygon.vertexlist[j] = (int)face[j];
        }
    }

    char switches[] = "pYzFQ";
    tetgenio tetgen_out;
    try {
        ::tetrahedralize(switches, &tetgen_in, &tetgen_out);
    } catch (const int e) {
        fflush(stdout);
        fflush(stderr);
        ABORT_F("TetGen failed: %i", e);
    }

#ifdef FOUR_DEBUG
    fflush(stdout);
    fflush(stderr);
#endif

    for (int i = 0; i < tetgen_out.numberofpoints; i++) {
        f64* point = &tetgen_out.pointlist[i * 3];
        out_vertices.push_back(glm::dvec3(point[0], point[1], point[2]));
    }

    CHECK_EQ_F(tetgen_out.numberofcorners, 4);
    for (int i = 0; i < tetgen_out.numberoftetrahedra * 4; i++) {
        out_tets.push_back((u32)tetgen_out.tetrahedronlist[i]);
    }
}

void tetrahedralize_cell(const Mesh4& mesh, const s64 cell_i, std::vector<glm::dvec4>& out_vertices,
                         std::vector<u32>& out_tets) {

    // Calculate normal vector

    const Cell& cell = mesh.cells[(size_t)cell_i];
    const Face& face0 = mesh.faces[cell[0]];
    u32 edge0_i = face0[0];
    const Edge& edge0 = mesh.edges[edge0_i];
    u32 v0_i = edge0.v0;
    glm::dvec4 v0 = mesh.vertices[v0_i];

    glm::dvec4 normal;
    {
        bool found_edge = false;
        u32 found_edge_i = (u32)-1;
        glm::dvec4 other_edges[2];

        for (u32 f_i : cell) {
            const Face& f = mesh.faces[f_i];
            for (u32 e_i : f) {

                if (e_i == edge0_i || (found_edge && e_i == found_edge_i)) {
                    continue;
                }

                const Edge& e = mesh.edges[e_i];
                if (e.v0 == v0_i || e.v1 == v0_i) {
                    u32 other_vi = e.v0 == v0_i ? e.v1 : e.v0;
                    if (!found_edge) {
                        found_edge = true;
                        found_edge_i = e_i;
                        other_edges[0] = mesh.vertices[other_vi] - v0;
                    } else {
                        other_edges[1] = mesh.vertices[other_vi] - v0;
                        goto find_v0_edges_end;
                    }
                }
            }
        }
        ABORT_F("Could not find normal vector");
    find_v0_edges_end:
        normal = glm::normalize(cross(mesh.vertices[edge0.v1] - v0, other_edges[0], other_edges[1]));
    }

    // Calculate transformation to 3D

    glm::dvec4 up;
    glm::dvec4 over;
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
        glm::dvec4 v0_ = transform(to_3d_trans_inverse * to_3d_trans, v0);
        DCHECK_F(float_eq(v0, v0_));
    }
#endif

    std::unordered_map<u32, u32> cell3_vertex_i_mapping;
    std::vector<glm::dvec3> cell3_vertices;
    std::vector<std::vector<u32>> cell3_faces;
    cell3_faces.reserve(cell.size());

    for (u32 f_i : cell) {
        const Face& f = mesh.faces[f_i];

        for (u32 e_i : f) {
            const Edge& e = mesh.edges[e_i];
            for (u32 v_i : e.vertices) {
                if (!has_key(cell3_vertex_i_mapping, v_i)) {
                    cell3_vertex_i_mapping.emplace(v_i, cell3_vertices.size());
                    glm::dvec4 v = mesh.vertices[v_i];
                    glm::dvec4 v_ = transform(to_3d_trans, v);
                    DCHECK_F(float_eq(v_.w, 0.0));
                    cell3_vertices.push_back(glm::dvec3(v_));
                }
            }
        }

        const Edge& e0 = mesh.edges[f[0]];
        u32 first_vi = e0.v0;
        std::vector<u32> this_mesh_f;
        this_mesh_f.reserve(f.size());
        this_mesh_f.push_back(cell3_vertex_i_mapping.at(first_vi));

        u32 prev_edge_i = f[0];
        u32 next_vi = e0.v1;

        while (next_vi != first_vi) {
            for (u32 e_i : f) {
                if (e_i != prev_edge_i) {
                    const Edge& e = mesh.edges[e_i];
                    if (e.v0 == next_vi || e.v1 == next_vi) {
                        this_mesh_f.push_back(cell3_vertex_i_mapping.at(next_vi));
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
        cell3_faces.push_back(std::move(this_mesh_f));
    }

    DCHECK_EQ_F(cell3_vertex_i_mapping.size(), cell3_vertices.size());

    glm::dmat4 temp_transform_inverse;
    {
        glm::dvec3 centroid = glm::dvec3(0, 0, 0);
        for (const auto& v : cell3_vertices) {
            centroid += v;
        }

        centroid.x /= (f64)cell3_vertices.size();
        centroid.y /= (f64)cell3_vertices.size();
        centroid.z /= (f64)cell3_vertices.size();

        glm::dmat4 temp_translate = translate(-1.0 * centroid);

        for (auto& v : cell3_vertices) {
            glm::dvec3 v_ = transform(temp_translate, v);
            v[0] = v_.x;
            v[1] = v_.y;
            v[2] = v_.z;
        }

        temp_transform_inverse = translate(centroid);
    }

#ifdef FOUR_DEBUG
    // All vertices of the cell should be on the same hyperplane
    for (const auto& entry : cell3_vertex_i_mapping) {
        if (entry.first != edge0.v0) {
            glm::dvec4 v = mesh.vertices[entry.first];
            f64 x = glm::dot(v - v0, normal);
            DCHECK_F(float_eq(x, 0.0));
        }
    }
#endif

    std::vector<glm::dvec3> out_vertices_3d;
    std::vector<u32> out_tets_3d;
    out_vertices_3d.reserve(cell3_vertices.size());
    tetrahedralize_polyhedron(cell3_vertices, cell3_faces, out_vertices_3d, out_tets_3d);

    std::unordered_map<u32, u32> tet_out_vertex_i_mapping;
    for (size_t i = 0; i < out_vertices_3d.size(); i++) {
        tet_out_vertex_i_mapping.emplace(i, out_vertices.size());
        const auto& v = out_vertices_3d[i];
        auto v_ = transform(temp_transform_inverse, v);
        out_vertices.push_back(transform(to_3d_trans_inverse, glm::dvec4(v_, 0.0)));
    }

    for (u32 i : out_tets_3d) {
        out_tets.push_back(tet_out_vertex_i_mapping.at(i));
    }
}
} // namespace

size_t FaceHash::operator()(const std::vector<u32>& x) const {
    this->x_ = x;
    std::sort(this->x_.begin(), this->x_.end());

    size_t hash = 0;
    for (u32 value : this->x_) {
        hash_combine(hash, value);
    }

    return hash;
}

bool FaceEquals::operator()(const std::vector<u32>& lhs, const std::vector<u32>& rhs) const {
    if (lhs.size() != rhs.size()) {
        return false;
    } else {
        for (u32 value : lhs) {
            if (!contains(rhs, value)) {
                return false;
            }
        }
        return true;
    }
}

bool operator==(const Edge& lhs, const Edge& rhs) {
    return (lhs.v0 == rhs.v0 && lhs.v1 == rhs.v1) || (lhs.v0 == rhs.v1 && lhs.v1 == rhs.v0);
}

void tetrahedralize(Mesh4& mesh) {
    mesh.tet_vertices.clear();
    mesh.tets.clear();

    std::vector<u32> out_tets;

    for (s64 cell_i = 0; cell_i < (s64)mesh.cells.size(); cell_i++) {
        const Cell& cell = mesh.cells[(size_t)cell_i];
        out_tets.clear();

        DCHECK_GE_F(cell.size(), 4u);
        if (cell.size() == 4) {
            // The cell is already a tetrahedron

            BoundedVector<u32, 4> vertex_indices;
            for (u32 f_i : cell) {
                const Face& f = mesh.faces[f_i];
                for (u32 e_i : f) {
                    const Edge& e = mesh.edges[e_i];
                    for (u32 v_i : e.vertices) {
                        if (!contains(vertex_indices, v_i)) {
                            vertex_indices.push_back(v_i);
                            out_tets.push_back((u32)mesh.tet_vertices.size());
                            mesh.tet_vertices.push_back(mesh.vertices[v_i]);
                        }
                    }
                }
            }

        } else {
            LOG_F(1, "Tetrahedralizing cell %li with %lu faces", cell_i, cell.size());
            tetrahedralize_cell(mesh, cell_i, mesh.tet_vertices, out_tets);
        }

        DCHECK_EQ_F((s64)out_tets.size() % 4, 0);
        for (size_t tet_i = 0; tet_i < out_tets.size() / 4; tet_i++) {
            Mesh4::Tet tet;
            tet.cell = (u32)cell_i;
            for (size_t j = 0; j < 4; j++) {
                tet.vertices[j] = out_tets[tet_i * 4 + j];
            }
            mesh.tets.push_back(tet);
        }
    }
}

bool save_mesh_to_file(const Mesh4& mesh, const char* path) {
    txml::XMLDocument doc;
    doc.InsertFirstChild(doc.NewDeclaration());
    txml::XMLElement* root = doc.NewElement("mesh4");
    doc.InsertEndChild(root);

    txml::XMLElement* vertices_xmle = doc.NewElement("vertices");
    root->InsertFirstChild(vertices_xmle);

    for (const auto& v : mesh.vertices) {
        txml::XMLElement* v_xmle = doc.NewElement("vec4");
        v_xmle->SetAttribute("x", v.x);
        v_xmle->SetAttribute("y", v.y);
        v_xmle->SetAttribute("z", v.z);
        v_xmle->SetAttribute("w", v.w);
        vertices_xmle->InsertEndChild(v_xmle);
    }

    txml::XMLElement* edges_xmle = doc.NewElement("edges");
    root->InsertEndChild(edges_xmle);

    for (const auto& e : mesh.edges) {
        txml::XMLElement* e_xmle = doc.NewElement("edge");
        e_xmle->SetAttribute("v0", e.v0);
        e_xmle->SetAttribute("v1", e.v1);
        edges_xmle->InsertEndChild(e_xmle);
    }

    txml::XMLElement* faces_xmle = doc.NewElement("faces");
    root->InsertEndChild(faces_xmle);

    for (const auto& f : mesh.faces) {
        txml::XMLElement* indices_xmle = doc.NewElement("indices");
        for (u32 index : f) {
            txml::XMLElement* index_xmle = doc.NewElement("index");
            index_xmle->SetText(index);
            indices_xmle->InsertEndChild(index_xmle);
        }
        faces_xmle->InsertEndChild(indices_xmle);
    }

    txml::XMLElement* cells_xmle = doc.NewElement("cells");
    root->InsertEndChild(cells_xmle);

    for (const auto& c : mesh.cells) {
        txml::XMLElement* indices_xmle = doc.NewElement("indices");
        for (u32 index : c) {
            txml::XMLElement* index_xmle = doc.NewElement("index");
            index_xmle->SetText(index);
            indices_xmle->InsertEndChild(index_xmle);
        }
        cells_xmle->InsertEndChild(indices_xmle);
    }

    txml::XMLElement* tet_vertices_xmle = doc.NewElement("tet_vertices");
    root->InsertEndChild(tet_vertices_xmle);

    for (const auto& v : mesh.tet_vertices) {
        txml::XMLElement* v_xmle = doc.NewElement("vec4");
        v_xmle->SetAttribute("x", v.x);
        v_xmle->SetAttribute("y", v.y);
        v_xmle->SetAttribute("z", v.z);
        v_xmle->SetAttribute("w", v.w);
        tet_vertices_xmle->InsertEndChild(v_xmle);
    }

    txml::XMLElement* tets_xmle = doc.NewElement("tets");
    root->InsertEndChild(tets_xmle);

    for (const Mesh4::Tet& tet : mesh.tets) {
        txml::XMLElement* tet_xmle = doc.NewElement("tet");
        tet_xmle->SetAttribute("cell", tet.cell);
        tet_xmle->SetAttribute("v0", tet.vertices[0]);
        tet_xmle->SetAttribute("v1", tet.vertices[1]);
        tet_xmle->SetAttribute("v2", tet.vertices[2]);
        tet_xmle->SetAttribute("v3", tet.vertices[3]);
        tets_xmle->InsertEndChild(tet_xmle);
    }

    txml::XMLError error = doc.SaveFile(path);
    if (error) {
        LOG_F(ERROR, "%s", txml::XMLDocument::ErrorIDToName(error));
        return false;
    }

    return true;
}

// FIXME: Don't crash if the file is not valid.
Mesh4 load_mesh_from_file(const char* path) {
    txml::XMLDocument doc;
    txml::XMLError error = doc.LoadFile(path);
    if (error) {
        ABORT_F("%s", txml::XMLDocument::ErrorIDToName(error));
    }

    Mesh4 result;

    txml::XMLElement* root = doc.RootElement();
    CHECK_F(c_str_eq(root->Name(), "mesh4"));

    txml::XMLElement* vertices_xmle = root->FirstChildElement();
    CHECK_F(c_str_eq(vertices_xmle->Name(), "vertices"));

    for (txml::XMLElement* v_xmle = vertices_xmle->FirstChildElement(); v_xmle != NULL;
         v_xmle = v_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(v_xmle->Name(), "vec4"));

        glm::dvec4 v = {};
        v_xmle->QueryDoubleAttribute("x", &v.x);
        v_xmle->QueryDoubleAttribute("y", &v.y);
        v_xmle->QueryDoubleAttribute("z", &v.z);
        v_xmle->QueryDoubleAttribute("w", &v.w);
        result.vertices.push_back(v);
    }

    txml::XMLElement* edges_xmle = vertices_xmle->NextSiblingElement();
    CHECK_F(c_str_eq(edges_xmle->Name(), "edges"));

    for (txml::XMLElement* e_xmle = edges_xmle->FirstChildElement(); e_xmle != NULL;
         e_xmle = e_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(e_xmle->Name(), "edge"));

        Edge e = {};
        e_xmle->QueryUnsignedAttribute("v0", &e.v0);
        e_xmle->QueryUnsignedAttribute("v1", &e.v1);
        result.edges.push_back(e);
    }

    txml::XMLElement* faces_xmle = edges_xmle->NextSiblingElement();
    CHECK_F(c_str_eq(faces_xmle->Name(), "faces"));

    for (txml::XMLElement* f_xmle = faces_xmle->FirstChildElement(); f_xmle != NULL;
         f_xmle = f_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(f_xmle->Name(), "indices"));

        std::vector<u32> face;

        for (txml::XMLElement* index_xmle = f_xmle->FirstChildElement(); index_xmle != NULL;
             index_xmle = index_xmle->NextSiblingElement()) {

            CHECK_F(c_str_eq(index_xmle->Name(), "index"));

            u32 value = 0;
            index_xmle->QueryUnsignedText(&value);
            face.push_back(value);
        }

        result.faces.push_back(std::move(face));
    }

    txml::XMLElement* cells_xmle = faces_xmle->NextSiblingElement();
    CHECK_F(c_str_eq(cells_xmle->Name(), "cells"));

    for (txml::XMLElement* c_xmle = cells_xmle->FirstChildElement(); c_xmle != NULL;
         c_xmle = c_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(c_xmle->Name(), "indices"));

        std::vector<u32> cell;

        for (txml::XMLElement* index_xmle = c_xmle->FirstChildElement(); index_xmle != NULL;
             index_xmle = index_xmle->NextSiblingElement()) {

            CHECK_F(c_str_eq(index_xmle->Name(), "index"));

            u32 value = 0;
            index_xmle->QueryUnsignedText(&value);
            cell.push_back(value);
        }

        result.cells.push_back(std::move(cell));
    }

    txml::XMLElement* tet_vertices_xmle = cells_xmle->NextSiblingElement();
    CHECK_F(c_str_eq(tet_vertices_xmle->Name(), "tet_vertices"));

    for (txml::XMLElement* v_xmle = tet_vertices_xmle->FirstChildElement(); v_xmle != NULL;
         v_xmle = v_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(v_xmle->Name(), "vec4"));

        glm::dvec4 v = {};
        v_xmle->QueryDoubleAttribute("x", &v.x);
        v_xmle->QueryDoubleAttribute("y", &v.y);
        v_xmle->QueryDoubleAttribute("z", &v.z);
        v_xmle->QueryDoubleAttribute("w", &v.w);
        result.tet_vertices.push_back(v);
    }

    txml::XMLElement* tets_xmle = tet_vertices_xmle->NextSiblingElement();
    CHECK_F(c_str_eq(tets_xmle->Name(), "tets"));

    for (txml::XMLElement* tet_xmle = tets_xmle->FirstChildElement(); tet_xmle != NULL;
         tet_xmle = tet_xmle->NextSiblingElement()) {

        CHECK_F(c_str_eq(tet_xmle->Name(), "tet"));

        Mesh4::Tet tet;
        tet_xmle->QueryUnsignedAttribute("cell", &tet.cell);
        tet_xmle->QueryUnsignedAttribute("v0", &tet.vertices[0]);
        tet_xmle->QueryUnsignedAttribute("v1", &tet.vertices[1]);
        tet_xmle->QueryUnsignedAttribute("v2", &tet.vertices[2]);
        tet_xmle->QueryUnsignedAttribute("v3", &tet.vertices[3]);
        result.tets.push_back(tet);
    }

    LOG_F(INFO, "Loaded Mesh4 from \"%s\" with %lu vertices, %lu edges, %lu faces, %lu cells.", path,
          result.vertices.size(), result.edges.size(), result.faces.size(), result.cells.size());
    return result;
}
} // namespace four
