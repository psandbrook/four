#include <four/mesh.hpp>

#include <loguru.hpp>
#include <tinyxml2.h>

#include <algorithm>
#include <string.h>
#include <utility>

namespace txml = tinyxml2;

namespace four {

size_t FaceHash::operator()(const std::vector<u32>& x) const {
    std::vector<u32> x_p(x);
    std::sort(x_p.begin(), x_p.end());

    size_t hash = 0;
    for (u32 value : x_p) {
        hash_combine(hash, value);
    }

    return hash;
}

bool FaceEquals::operator()(const std::vector<u32>& lhs, const std::vector<u32>& rhs) const {
    for (u32 value : lhs) {
        if (!contains(rhs, value)) {
            return false;
        }
    }

    return true;
}

bool operator==(const Edge& lhs, const Edge& rhs) {
    return (lhs.v0 == rhs.v0 && lhs.v1 == rhs.v1) || (lhs.v0 == rhs.v1 && lhs.v1 == rhs.v0);
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
        v_xmle->SetAttribute("x", v.X);
        v_xmle->SetAttribute("y", v.Y);
        v_xmle->SetAttribute("z", v.Z);
        v_xmle->SetAttribute("w", v.W);
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
    CHECK_EQ_F(strcmp(root->Name(), "mesh4"), 0);

    txml::XMLElement* vertices_xmle = root->FirstChildElement();
    CHECK_EQ_F(strcmp(vertices_xmle->Name(), "vertices"), 0);

    for (txml::XMLElement* v_xmle = vertices_xmle->FirstChildElement(); v_xmle != NULL;
         v_xmle = v_xmle->NextSiblingElement()) {

        CHECK_EQ_F(strcmp(v_xmle->Name(), "vec4"), 0);

        hmm_vec4 v;
        CHECK_EQ_F(v_xmle->QueryDoubleAttribute("x", &v.X), txml::XML_SUCCESS);
        CHECK_EQ_F(v_xmle->QueryDoubleAttribute("y", &v.Y), txml::XML_SUCCESS);
        CHECK_EQ_F(v_xmle->QueryDoubleAttribute("z", &v.Z), txml::XML_SUCCESS);
        CHECK_EQ_F(v_xmle->QueryDoubleAttribute("w", &v.W), txml::XML_SUCCESS);
        result.vertices.push_back(v);
    }

    txml::XMLElement* edges_xmle = vertices_xmle->NextSiblingElement();
    CHECK_EQ_F(strcmp(edges_xmle->Name(), "edges"), 0);

    for (txml::XMLElement* e_xmle = edges_xmle->FirstChildElement(); e_xmle != NULL;
         e_xmle = e_xmle->NextSiblingElement()) {

        CHECK_EQ_F(strcmp(e_xmle->Name(), "edge"), 0);

        Edge e;
        CHECK_EQ_F(e_xmle->QueryUnsignedAttribute("v0", &e.v0), txml::XML_SUCCESS);
        CHECK_EQ_F(e_xmle->QueryUnsignedAttribute("v1", &e.v1), txml::XML_SUCCESS);
        result.edges.push_back(e);
    }

    txml::XMLElement* faces_xmle = edges_xmle->NextSiblingElement();
    CHECK_EQ_F(strcmp(faces_xmle->Name(), "faces"), 0);

    for (txml::XMLElement* f_xmle = faces_xmle->FirstChildElement(); f_xmle != NULL;
         f_xmle = f_xmle->NextSiblingElement()) {

        CHECK_EQ_F(strcmp(f_xmle->Name(), "indices"), 0);

        std::vector<u32> face;

        for (txml::XMLElement* index_xmle = f_xmle->FirstChildElement(); index_xmle != NULL;
             index_xmle = index_xmle->NextSiblingElement()) {

            CHECK_EQ_F(strcmp(index_xmle->Name(), "index"), 0);

            u32 value;
            CHECK_EQ_F(index_xmle->QueryUnsignedText(&value), txml::XML_SUCCESS);
            face.push_back(value);
        }

        result.faces.push_back(std::move(face));
    }

    txml::XMLElement* cells_xmle = faces_xmle->NextSiblingElement();
    CHECK_EQ_F(strcmp(cells_xmle->Name(), "cells"), 0);

    for (txml::XMLElement* c_xmle = cells_xmle->FirstChildElement(); c_xmle != NULL;
         c_xmle = c_xmle->NextSiblingElement()) {

        CHECK_EQ_F(strcmp(c_xmle->Name(), "indices"), 0);

        std::vector<u32> cell;

        for (txml::XMLElement* index_xmle = c_xmle->FirstChildElement(); index_xmle != NULL;
             index_xmle = index_xmle->NextSiblingElement()) {

            CHECK_EQ_F(strcmp(index_xmle->Name(), "index"), 0);

            u32 value;
            CHECK_EQ_F(index_xmle->QueryUnsignedText(&value), txml::XML_SUCCESS);
            cell.push_back(value);
        }

        result.cells.push_back(std::move(cell));
    }

    return result;
}
} // namespace four
