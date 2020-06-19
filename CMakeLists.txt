cmake_minimum_required(VERSION 3.13.4 FATAL_ERROR)
project(four LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
elseif(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release"))
  message(FATAL_ERROR "Build type can only be \"Debug\" or \"Release\"")
endif()

# Dependencies
# ============

find_package(SDL2 2.0 REQUIRED)

add_library(glad STATIC depends/glad/src/glad.c)
target_compile_options(glad PRIVATE -w)

add_library(triangle STATIC depends/triangle/triangle.c)
add_compile_definitions(LINUX TRILIBRARY ANSI_DECLARATORS)
target_compile_options(triangle PRIVATE -w -fno-lto)

add_library(loguru STATIC depends/loguru/loguru.cpp)
target_compile_options(loguru PRIVATE -w)

add_library(tinyxml2 STATIC depends/tinyxml2/tinyxml2.cpp)
target_compile_options(tinyxml2 PRIVATE -w)

# ============

# Prefix: src/
set(SOURCES
  main.cpp
  impl.cpp
  four/generate.cpp
  four/mesh.cpp
)

# Add prefix
list(TRANSFORM SOURCES PREPEND "src/")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
  -DFOUR_DEBUG -Og -ggdb3 -fvar-tracking -fvar-tracking-assignments -fsanitize=undefined
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
target_link_libraries(four rt dl m pthread SDL2::SDL2 glad triangle loguru tinyxml2)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET four
    POST_BUILD
    COMMAND strip --strip-unneeded "$<TARGET_FILE:four>"
    VERBATIM
  )
endif()
