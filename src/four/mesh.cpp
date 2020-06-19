#include <four/mesh.hpp>

#include <algorithm>

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
} // namespace four
