cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(four LANGUAGES C CXX)

include(FetchContent)

function(make_available_no_add)
  foreach(CONTENT_NAME ${ARGV})
    FetchContent_GetProperties(${CONTENT_NAME})
    if(NOT ${CONTENT_NAME}_POPULATED)
      FetchContent_Populate(${CONTENT_NAME})
    endif()
    set(${CONTENT_NAME}_SOURCE_DIR ${${CONTENT_NAME}_SOURCE_DIR} PARENT_SCOPE)
  endforeach()
endfunction()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
elseif(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release"))
  message(FATAL_ERROR "Build type can only be \"Debug\" or \"Release\"")
endif()

set(CMAKE_TLS_VERIFY ON)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

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

FetchContent_Declare(
  sdl2
  URL "https://www.libsdl.org/release/SDL2-2.0.12.tar.gz"
  URL_HASH SHA256=349268f695c02efbc9b9148a70b85e58cefbbf704abd3e91be654db7f1e2c863
)

FetchContent_Declare(
  loguru
  URL "https://github.com/emilk/loguru/archive/v2.1.0.tar.gz"
  URL_HASH SHA256=1a3be62ebec5609af60b1e094109a93b7412198b896bb88f31dcfe4d95b79ce7
)

FetchContent_Declare(
  eigen
  URL "https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.bz2"
  URL_HASH SHA256=685adf14bd8e9c015b78097c1dc22f2f01343756f196acdc76a678e1ae352e11
)

FetchContent_Declare(
  libigl
  URL "https://github.com/libigl/libigl/archive/v2.1.0.tar.gz"
  URL_HASH SHA256=816ec64b6b2fbabcb1c6353c7df8a2fc1ad5bf4c607b332871a6ab8b29c89dbc
)

FetchContent_Declare(
  tinyxml2
  URL "https://github.com/leethomason/tinyxml2/archive/7.1.0.tar.gz"
  URL_HASH SHA256=68ebd396a4220d5a9b5a621c6e9c66349c5cfdf5efaea3f16e3bb92e45f4e2a3
)

FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/v1.74.tar.gz"
  URL_HASH SHA256=2f5f2b789edb00260aa71f03189da5f21cf4b5617c4fbba709e9fbcfc76a2f1e
)

make_available_no_add(sdl2 loguru eigen libigl tinyxml2 imgui)

set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
add_subdirectory("${sdl2_SOURCE_DIR}")
target_compile_options(SDL2-static PRIVATE -w -fno-lto)

add_library(glad STATIC depends/glad/src/glad.c)
target_compile_options(glad PRIVATE -w)

add_library(triangle STATIC depends/triangle/triangle.c)
target_compile_definitions(triangle PRIVATE LINUX TRILIBRARY)
target_compile_options(triangle PRIVATE -w -fno-lto)

add_library(loguru STATIC "${loguru_SOURCE_DIR}/loguru.cpp")
target_compile_options(loguru PRIVATE -w)

add_library(tinyxml2 STATIC "${tinyxml2_SOURCE_DIR}/tinyxml2.cpp")
target_compile_options(tinyxml2 PRIVATE -w)

add_library(imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/examples/imgui_impl_sdl.cpp"
  "${imgui_SOURCE_DIR}/examples/imgui_impl_opengl3.cpp"
)
target_compile_options(imgui PRIVATE -w -fno-lto)
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui SDL2-static)

add_library(tetgen STATIC depends/tetgen/predicates.cxx depends/tetgen/tetgen.cxx)
target_compile_options(tetgen PRIVATE -w)
add_compile_definitions(TETLIBRARY)

include_directories(SYSTEM
  depends/glad/include
  depends/Handmade-Math
  depends/triangle
  depends/tetgen
  "${libigl_SOURCE_DIR}/include"
  "${eigen_SOURCE_DIR}"
  "${loguru_SOURCE_DIR}"
  "${tinyxml2_SOURCE_DIR}"
  "${imgui_SOURCE_DIR}"
  "${imgui_SOURCE_DIR}/examples"
)

# ============

target_link_libraries(four rt dl m pthread SDL2-static glad triangle loguru tinyxml2 imgui tetgen)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET four
    POST_BUILD
    COMMAND strip --strip-unneeded "$<TARGET_FILE:four>"
    VERBATIM
  )
endif()
