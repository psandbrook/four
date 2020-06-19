cmake_minimum_required(VERSION 3.13.4 FATAL_ERROR)
project(four LANGUAGES C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")
include(cotire)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
elseif(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release"))
  message(FATAL_ERROR "Build type can only be \"Debug\" or \"Release\"")
endif()

# Prefix: src/
set(SOURCES
  main.cpp
  impl.cpp
  four/generate.cpp
  four/mesh.cpp
  four/render.cpp
  four/app_state.cpp
  four/render_funcs.cpp
)

# Add prefix
list(TRANSFORM SOURCES PREPEND "src/")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include_directories(src)

include_directories(SYSTEM
  depends/glad/include
  depends/Handmade-Math
  depends/libigl/include
  depends/eigen
  depends/triangle
  depends/loguru
  depends/tinyxml2
  depends/imgui
  depends/tetgen
)

set(CXX_FLAGS_COMMON
  -Wall -Wextra -Wcast-align=strict -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self
  -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wundef -Wfloat-equal
  -Wstack-protector -Wwrite-strings -Wrestrict -Wvla -Wuninitialized -Wctor-dtor-privacy -Wnoexcept
  -Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -Wuseless-cast -Wabi-tag -Wconversion

  -pthread -pipe -fPIE -pie -fvisibility=hidden -fvisibility-inlines-hidden -fstack-protector-strong -fno-plt
  -fno-strict-aliasing -Wl,--sort-common,--as-needed -z relro -z now -m64 -march=x86-64 -mtune=generic
)

set(CXX_FLAGS_DEBUG
  #-DFOUR_DEBUG -Og -g -fvar-tracking -fvar-tracking-assignments -fsanitize=undefined
  -DFOUR_DEBUG -O0 -g -fsanitize=undefined
)

set(CXX_FLAGS_RELEASE
  -Wno-unused -O3 -g0 -DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections -Wl,--gc-sections,-O1 -flto
  -fuse-linker-plugin -flto-odr-type-merging -fno-semantic-interposition
)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  list(APPEND CXX_FLAGS_COMMON -fdiagnostics-color=always)
endif()

set(C_FLAGS_COMMON ${CXX_FLAGS_COMMON})
list(REMOVE_ITEM C_FLAGS_COMMON
  -Wctor-dtor-privacy -Wnoexcept -Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -Wuseless-cast -Wabi-tag
  -fvisibility-inlines-hidden
)

set(C_FLAGS_DEBUG ${CXX_FLAGS_DEBUG})
set(C_FLAGS_RELEASE ${CXX_FLAGS_RELEASE})

string(REPLACE ";" " " CXX_FLAGS_COMMON_STR "${CXX_FLAGS_COMMON}")
string(REPLACE ";" " " CXX_FLAGS_DEBUG_STR "${CXX_FLAGS_DEBUG}")
string(REPLACE ";" " " CXX_FLAGS_RELEASE_STR "${CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS ${CXX_FLAGS_COMMON_STR})
set(CMAKE_CXX_FLAGS_DEBUG ${CXX_FLAGS_DEBUG_STR})
set(CMAKE_CXX_FLAGS_RELEASE ${CXX_FLAGS_RELEASE_STR})

string(REPLACE ";" " " C_FLAGS_COMMON_STR "${C_FLAGS_COMMON}")
string(REPLACE ";" " " C_FLAGS_DEBUG_STR "${C_FLAGS_DEBUG}")
string(REPLACE ";" " " C_FLAGS_RELEASE_STR "${C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS ${C_FLAGS_COMMON_STR})
set(CMAKE_C_FLAGS_DEBUG ${C_FLAGS_DEBUG_STR})
set(CMAKE_C_FLAGS_RELEASE ${C_FLAGS_RELEASE_STR})

add_executable(four ${SOURCES})

# Dependencies
# ============

set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
add_subdirectory(depends/SDL2)
target_compile_options(SDL2-static PRIVATE -w -fno-lto)

add_library(glad STATIC depends/glad/src/glad.c)
target_compile_options(glad PRIVATE -w)

add_library(triangle STATIC depends/triangle/triangle.c)
target_compile_definitions(triangle PRIVATE LINUX TRILIBRARY)
target_compile_options(triangle PRIVATE -w -fno-lto)

add_library(loguru STATIC depends/loguru/loguru.cpp)
target_compile_options(loguru PRIVATE -w)

add_library(tinyxml2 STATIC depends/tinyxml2/tinyxml2.cpp)
target_compile_options(tinyxml2 PRIVATE -w)

add_library(imgui STATIC
  depends/imgui/imgui.cpp
  depends/imgui/imgui_demo.cpp
  depends/imgui/imgui_draw.cpp
  depends/imgui/imgui_widgets.cpp
  depends/imgui/imgui_impl_sdl.cpp
  depends/imgui/imgui_impl_opengl3.cpp
)
target_compile_options(imgui PRIVATE -w -fno-lto)
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui SDL2-static)

add_library(tetgen STATIC depends/tetgen/predicates.cxx depends/tetgen/tetgen.cxx)
target_compile_options(tetgen PRIVATE -w)
add_compile_definitions(TETLIBRARY)

# ============

target_link_libraries(four rt dl m pthread SDL2-static glad triangle loguru tinyxml2 imgui tetgen)

set_property(TARGET four PROPERTY COTIRE_PREFIX_HEADER_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/depends)
set_property(TARGET four PROPERTY COTIRE_PREFIX_HEADER_IGNORE_PATH
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/depends/triangle/triangle.h
  ${CMAKE_SOURCE_DIR}/depends/Handmade-Math/HandmadeMath.h
)
cotire(four CONFIGURATIONS Debug)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET four
    POST_BUILD
    COMMAND strip --strip-unneeded "$<TARGET_FILE:four>"
    VERBATIM
  )
endif()
