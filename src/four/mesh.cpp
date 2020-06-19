#include <four/mesh.hpp>

#include <algorithm>

namespace four {

size_t FaceHash::operator()(const std::vector<uint32_t>& x) const {
    std::vector<uint32_t> x_p(x);
    std::sort(x_p.begin(), x_p.end());

    size_t hash = 0;
    for (uint32_t value : x_p) {
        hash_combine(hash, value);
    }

    return hash;
}

bool FaceEquals::operator()(const std::vector<uint32_t>& lhs, const std::vector<uint32_t>& rhs) const {
    for (uint32_t value : lhs) {
        if (!contains(rhs, value)) {
            return false;
        }
    }

    return true;
}

bool operator==(const Edge& lhs, const Edge& rhs) {
    return (lhs.v1 == rhs.v1 && lhs.v2 == rhs.v2) || (lhs.v1 == rhs.v2 && lhs.v2 == rhs.v1);
}
} // namespace four
