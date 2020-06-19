#pragma once

#include <four/mesh.hpp>

#include <loguru.hpp>

namespace four {

// Regular convex 4-polytopes
enum class RcMesh4Type {
    n5_cell,
    tesseract,
    n16_cell,
    n24_cell,
    n120_cell,
    n600_cell,
    count,
};

inline const char* rc_mesh4_type_str[] = {
        "5-cell", "tesseract", "16-cell", "24-cell", "120-cell", "600-cell",
};

inline RcMesh4Type rc_mesh4_type(const char* str) {
    for (s32 i = 0; i < (s32)RcMesh4Type::count; i++) {
        if (strcmp(str, rc_mesh4_type_str[i]) == 0) {
            return (RcMesh4Type)i;
        }
    }
    ABORT_F("Unknown `RcMesh4Type`: %s", str);
}

Mesh4 generate_5cell();
Mesh4 generate_tesseract();
Mesh4 generate_16cell();
Mesh4 generate_24cell();
Mesh4 generate_120cell();
Mesh4 generate_600cell();

inline Mesh4 generate_regular_convex_mesh4(RcMesh4Type type) {
    switch (type) {
    case RcMesh4Type::n5_cell:
        return generate_5cell();

    case RcMesh4Type::tesseract:
        return generate_tesseract();

    case RcMesh4Type::n16_cell:
        return generate_16cell();

    case RcMesh4Type::n24_cell:
        return generate_24cell();

    case RcMesh4Type::n120_cell:
        return generate_120cell();

    case RcMesh4Type::n600_cell:
        return generate_600cell();

    default:
        ABORT_F("Unknown RcMesh4Type");
    }
}
} // namespace four
