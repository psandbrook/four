#pragma once

#include <four/utility.hpp>

#include <string>

namespace four {

void init_resource_path();

// Returns an absolute path to the specified resource, relative to the
// application's data directory.
std::string get_resource_path(const char* relative_path);

} // namespace four
