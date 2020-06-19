#include <four/utility.hpp>

#include <float.h>
#include <math.h>

namespace four {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

bool float_eq(float a, float b, float epsilon) {
    // Code adapted from https://floating-point-gui.de/

    float diff = fabsf(a - b);
    if (a == b) {
        // Shortcut, handles infinities
        return true;
    } else if (a == 0 || b == 0 || diff < FLT_MIN) {
        // a or b is zero or both are extremely close to it. Relative error is
        // less meaningful here.
        return diff < epsilon * FLT_MIN;
    } else {
        // Use relative error
        // FIXME: Does `fminf` do the right thing here?
        return diff / fminf(fabsf(a) + fabsf(b), FLT_MAX) < epsilon;
    }
}
#pragma GCC diagnostic pop

} // namespace four
