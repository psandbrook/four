#include <four/generate.hpp>

#define HANDMADE_MATH_IMPLEMENTATION
#include <HandmadeMath.h>

int main() {
    four::Mesh4 tesseract = four::generate_tesseract();

    printf("Vertices: %lu\n", tesseract.vertices.size());
    printf("Edges: %lu\n", tesseract.edges.size());
    printf("Faces: %lu\n", tesseract.faces.size());
    printf("Cells: %lu\n", tesseract.cells.size());

    return 0;
}
