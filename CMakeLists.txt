cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(four VERSION 0.1.0 LANGUAGES C CXX)

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

# === Set global variables ===

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install_prefix" CACHE PATH "" FORCE)
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")
set(CMAKE_DEBUG_POSTFIX "+debug")
set(CMAKE_TLS_VERIFY ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

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
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

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

set(COMPILE_FLAGS_RELEASE
  -O3 -g0 -DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections
  "-ffile-prefix-map=${FETCHCONTENT_BASE_DIR}=_deps" "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
)

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

if(WIN32)
  list(APPEND FLAGS_FOUR_COMMON -Wno-format -Wno-redundant-decls)
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

set(CMAKE_SKIP_INSTALL_RULES TRUE)
add_subdirectory("${sdl2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sdl2")
set(CMAKE_SKIP_INSTALL_RULES FALSE)
file(TOUCH "${CMAKE_BINARY_DIR}/sdl2/cmake_install.cmake")

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
target_compile_options(loguru PRIVATE -w)

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

set_property(TARGET four PROPERTY INSTALL_RPATH "$ORIGIN")

file(CREATE_LINK "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data" RESULT DATA_LINK_RESULT SYMBOLIC)
if(DATA_LINK_RESULT)
  file(COPY "${CMAKE_SOURCE_DIR}/data" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

if(WIN32)
  set(WIN32_REQUIRED_DLLS_NAMES libgcc_s_seh-1.dll libssp-0.dll libstdc++-6.dll libwinpthread-1.dll)
  set(WIN32_REQUIRED_DLLS)
  foreach(DLL_NAME ${WIN32_REQUIRED_DLLS_NAMES})
    find_file("${DLL_NAME}_LOCATION" "${DLL_NAME}" PATHS /bin /lib)
    list(APPEND WIN32_REQUIRED_DLLS "${${DLL_NAME}_LOCATION}")
  endforeach()
  file(COPY ${WIN32_REQUIRED_DLLS} DESTINATION "${CMAKE_BINARY_DIR}")
endif()

# === Installation and packaging ===

string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME_LOWER)
set(PACKAGE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}$<$<CONFIG:Debug>:${CMAKE_DEBUG_POSTFIX}>-${SYSTEM_NAME_LOWER}")

if(UNIX)
  set(PACKAGE_OUTPUT "${CMAKE_BINARY_DIR}/${PACKAGE_NAME}.tar.gz")
elseif(WIN32)
  set(PACKAGE_OUTPUT "${CMAKE_BINARY_DIR}/${PACKAGE_NAME}.zip")
endif()

install(TARGETS four SDL2 LIBRARY DESTINATION four RUNTIME DESTINATION four)

install(CODE
  "execute_process(COMMAND \
    \"$<IF:$<CONFIG:Release>,${CMAKE_STRIP},true>\" \
    --strip-unneeded \
    \"four/$<TARGET_FILE_NAME:four>\" \
    \"four/$<TARGET_FILE_NAME:SDL2>\" \
    WORKING_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")"
)

install(DIRECTORY data DESTINATION four)

if(WIN32)
  install(PROGRAMS ${WIN32_REQUIRED_DLLS} DESTINATION four)
endif()

add_custom_target(pre_package
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}"
  COMMAND "${CMAKE_COMMAND}" -E remove -f "${CMAKE_BINARY_DIR}/install_manifest.txt" "${PACKAGE_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target install
  VERBATIM
)

if(UNIX)
  add_custom_target(package
    tar -v --format=posix -I "gzip -cfn" -cf "${PACKAGE_OUTPUT}" four
    WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    VERBATIM
  )
elseif(WIN32)
  add_custom_target(package
    zip -vro "${PACKAGE_OUTPUT}" four
    WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    VERBATIM
  )
endif()

add_dependencies(package pre_package)
