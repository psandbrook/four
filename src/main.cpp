#include <four/generate.hpp>

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

int main() {
    four::Mesh4 mesh = four::generate_tesseract();
    printf("Vertices: %lu\n", mesh.vertices.size());
    printf("Edges: %lu\n", mesh.edges.size());
    printf("Faces: %lu\n", mesh.faces.size());
    printf("Cells: %lu\n", mesh.cells.size());

    return 0;
}
