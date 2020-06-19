#include <four/mesh.hpp>

#include <algorithm>

namespace four {

size_t FaceHash::operator()(const std::unordered_set<uint32_t>& x) const {
    std::vector<uint32_t> ordered{x.cbegin(), x.cend()};
    std::sort(ordered.begin(), ordered.end());

    size_t hash = 0;
    for (uint32_t val : ordered) {
        four::hash_combine(hash, val);
    }

    return hash;
}

bool operator==(const Edge& lhs, const Edge& rhs) {
    return (lhs.v1 == rhs.v1 && lhs.v2 == rhs.v2) || (lhs.v1 == rhs.v2 && lhs.v2 == rhs.v1);
}
} // namespace four
