#pragma once

#include <four/mesh.hpp>

#include <memory>

namespace four {

struct TriangulateFn {
private:
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    TriangulateFn();
    ~TriangulateFn();

    void operator()(const std::vector<hmm_vec3>& vertices, const std::vector<Edge>& edges, const Face& face,
                    std::vector<u32>& out);
};
} // namespace four
