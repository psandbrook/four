#include <four/resource.hpp>

#include <SDL.h>
#include <loguru.hpp>

namespace four {

namespace {

char* exe_dir = nullptr;
std::string resource_dir;

} // namespace

void init_resource_path() {
    if (exe_dir) {
        return;
    }

    exe_dir = SDL_GetBasePath();
    if (!exe_dir) {
        ABORT_F("SDL_GetBasePath() failed");
    }

    resource_dir = std::string(exe_dir);
    resource_dir.append("/data");
}

std::string get_resource_path(const char* relative_path) {
    std::string result = resource_dir;
    result.push_back('/');
    result.append(relative_path);
    return result;
}
} // namespace four
