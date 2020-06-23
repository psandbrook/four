cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(four LANGUAGES C CXX)

include(FetchContent)
include(ProcessorCount)
include(CheckPIESupported)

function(make_available_no_add)
  foreach(CONTENT_NAME ${ARGV})
    FetchContent_GetProperties(${CONTENT_NAME})
    if(NOT ${CONTENT_NAME}_POPULATED)
      FetchContent_Populate(${CONTENT_NAME})
    endif()
    set(${CONTENT_NAME}_SOURCE_DIR ${${CONTENT_NAME}_SOURCE_DIR} PARENT_SCOPE)
  endforeach()
endfunction()

ProcessorCount(NPROC)
if(NPROC EQUAL 0)
  set(NPROC 1)
endif()

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  message(FATAL_ERROR "Unsupported compiler")
endif()

# === Options ===

option(DEBUG_ENABLE_UBSAN "Enable undefined behaviour sanitizer in debug builds.")
option(DEBUG_ENABLE_ASAN "Enable address sanitizer in debug builds.")

# This is off by default to reduce compile times.
option(DEBUG_ENABLE_DEBUG_INFO "Enable debug information in debug builds.")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release." FORCE)
endif()

set(COMPILER_LAUNCHER "" CACHE STRING "C and CXX compiler launcher.")

# === Set global variables ===

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")
set(CMAKE_TLS_VERIFY ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

if(COMPILER_LAUNCHER)
  set(CMAKE_C_COMPILER_LAUNCHER ${COMPILER_LAUNCHER})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${COMPILER_LAUNCHER})
endif()

# === Source files ===

# Prefix: src/
set(SOURCES
  main.cpp
  four/generate.cpp
  four/mesh.cpp
  four/render.cpp
  four/app_state.cpp
  four/resource.cpp
)

# Add prefix
list(TRANSFORM SOURCES PREPEND "src/")

# === Compile and link flags ===

unset(ENV{CPPFLAGS})
unset(ENV{CFLAGS})
unset(ENV{CXXFLAGS})
unset(ENV{LDFLAGS})
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

check_pie_supported()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Flags for all targets, including dependencies

set(COMPILE_FLAGS_COMMON
  -pthread -pipe -fvisibility=hidden $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>
  -fstack-protector-strong -fno-plt -m64 -march=x86-64 -mtune=generic
)

set(LINK_FLAGS_COMMON -Wl,--sort-common,--as-needed)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  list(APPEND COMPILE_FLAGS_COMMON -fdiagnostics-color=always)
endif()

if(UNIX)
  list(APPEND LINK_FLAGS_COMMON -Wl,-z,relro,-z,now)
endif()

set(FLAGS_DEBUG -Og -g)

set(COMPILE_FLAGS_RELEASE -O3 -g0 -DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections)
set(LINK_FLAGS_RELEASE -Wl,--gc-sections,-O1)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND COMPILE_FLAGS_RELEASE -flto=${NPROC} -flto-odr-type-merging -fno-semantic-interposition)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND COMPILE_FLAGS_RELEASE -flto=thin)
endif()

# Flags for this project's targets

set(FLAGS_FOUR_COMMON
  -Wall -Wextra -Wpedantic -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations
  -Wmissing-include-dirs -Wredundant-decls -Wundef -Wfloat-equal -Wstack-protector -Wwrite-strings -Wvla -Wuninitialized
  -Wctor-dtor-privacy -Woverloaded-virtual -Wsign-promo -Wconversion -Wstrict-aliasing=1
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND FLAGS_FOUR_COMMON -Wcast-align=strict -Wlogical-op -Wstrict-null-sentinel -Wuseless-cast)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND FLAGS_FOUR_COMMON -Wcast-align -Wno-gnu-zero-variadic-macro-arguments)
endif()

set(FLAGS_FOUR_DEBUG -DFOUR_DEBUG)

if(NOT DEBUG_ENABLE_DEBUG_INFO)
  list(APPEND FLAGS_FOUR_DEBUG -O0 -g0)
endif()

if(DEBUG_ENABLE_UBSAN)
  list(APPEND FLAGS_FOUR_DEBUG -fsanitize=undefined)
endif()

if(DEBUG_ENABLE_ASAN)
  list(APPEND FLAGS_FOUR_DEBUG -fsanitize=address)
endif()

set(FLAGS_FOUR_RELEASE -Wno-unused -Wno-unused-parameter)

add_compile_options(
  ${COMPILE_FLAGS_COMMON}
  "$<$<CONFIG:Debug>:${FLAGS_DEBUG}>"
  "$<$<CONFIG:Release>:${COMPILE_FLAGS_RELEASE}>"
)
add_link_options(
  ${COMPILE_FLAGS_COMMON}
  ${LINK_FLAGS_COMMON}
  "$<$<CONFIG:Debug>:${FLAGS_DEBUG}>"
  "$<$<CONFIG:Release>:${COMPILE_FLAGS_RELEASE}>"
  "$<$<CONFIG:Release>:${LINK_FLAGS_RELEASE}>"
)

# === Dependencies ===

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
  tinyxml2
  URL "https://github.com/leethomason/tinyxml2/archive/7.1.0.tar.gz"
  URL_HASH SHA256=68ebd396a4220d5a9b5a621c6e9c66349c5cfdf5efaea3f16e3bb92e45f4e2a3
)

FetchContent_Declare(
  imgui
  URL "https://github.com/ocornut/imgui/archive/v1.74.tar.gz"
  URL_HASH SHA256=2f5f2b789edb00260aa71f03189da5f21cf4b5617c4fbba709e9fbcfc76a2f1e
)

FetchContent_Declare(
  earcut
  URL "https://github.com/mapbox/earcut.hpp/archive/5d554768316a7931bcc999dc8d7675d58be1ac45.zip"
  URL_HASH SHA256=8bfe45bc04df9b8fab4485c1fe09b9bf9a25e9998ce8133e2506a98863aa826c
)

FetchContent_Declare(
  glm
  URL "https://github.com/g-truc/glm/releases/download/0.9.9.7/glm-0.9.9.7.7z"
  URL_HASH SHA256=9a2c05e0ca77139b43949ef7c3780854dcedaa6590a15ddc7532244469fb4fdf
)

make_available_no_add(sdl2 loguru tinyxml2 imgui earcut glm)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory("${sdl2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sdl2")

target_compile_options(SDL2 PRIVATE -fno-lto)

add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE "${glm_SOURCE_DIR}")

add_library(glad STATIC depends/glad/src/glad.c)
target_include_directories(glad SYSTEM PUBLIC depends/glad/include)

if(UNIX)
  target_link_libraries(glad dl)
endif()

add_library(earcut INTERFACE)
target_include_directories(earcut SYSTEM INTERFACE "${earcut_SOURCE_DIR}/include/mapbox")

add_library(imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/examples/imgui_impl_sdl.cpp"
  "${imgui_SOURCE_DIR}/examples/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/examples")
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui SDL2 glad)
set_property(SOURCE "${imgui_SOURCE_DIR}/imgui_draw.cpp" PROPERTY COMPILE_OPTIONS -fno-lto)

add_library(loguru STATIC "${loguru_SOURCE_DIR}/loguru.cpp")
target_include_directories(loguru SYSTEM PUBLIC "${loguru_SOURCE_DIR}")
target_compile_definitions(loguru PUBLIC LOGURU_WITH_STREAMS)

if(UNIX)
  target_link_libraries(loguru dl)
endif()

add_library(tinyxml2 STATIC "${tinyxml2_SOURCE_DIR}/tinyxml2.cpp")
target_include_directories(tinyxml2 SYSTEM PUBLIC "${tinyxml2_SOURCE_DIR}")

add_library(tetgen STATIC depends/tetgen/tetgen.cxx depends/tetgen/predicates.cxx)
target_include_directories(tetgen SYSTEM PUBLIC depends/tetgen)
target_compile_definitions(tetgen PUBLIC TETLIBRARY)
target_compile_options(tetgen PRIVATE -Wno-unused-result)

# === Project targets ===

add_executable(four ${SOURCES})
target_compile_options(four PRIVATE
  ${FLAGS_FOUR_COMMON}
  "$<$<CONFIG:Debug>:${FLAGS_FOUR_DEBUG}>"
  "$<$<CONFIG:Release>:${FLAGS_FOUR_RELEASE}>"
)
target_link_options(four PRIVATE
  ${FLAGS_FOUR_COMMON}
  "$<$<CONFIG:Debug>:${FLAGS_FOUR_DEBUG}>"
  "$<$<CONFIG:Release>:${FLAGS_FOUR_RELEASE}>"
)
include_directories(src)

if(WIN32)
  target_link_libraries(four mingw32 SDL2main)
endif()

target_link_libraries(four SDL2 glad glm loguru imgui tinyxml2 earcut tetgen)

add_custom_command(
  TARGET four
  POST_BUILD
  COMMAND "$<IF:$<CONFIG:Release>,${CMAKE_STRIP},:>" --strip-unneeded "$<TARGET_FILE:four>"
  VERBATIM
)

file(CREATE_LINK "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data" RESULT DATA_LINK_RESULT SYMBOLIC)
if(DATA_LINK_RESULT)
  file(COPY "${CMAKE_SOURCE_DIR}/data" DESTINATION "${CMAKE_BINARY_DIR}")
endif()
