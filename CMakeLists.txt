cmake_minimum_required(VERSION 3.20)

# CMake 4 removed implicit support for projects requiring policy < 3.5.
# Some transitive deps still ship those minimums; this lets them configure.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(HackathonGame CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# ---- GLFW ----------------------------------------------------------------
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# ---- GLAD (OpenGL 3.3 Core via glad2) ------------------------------------
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.6
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)

# ---- GLM (header only) ---------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ---- stb (single-header) -------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# ---- miniaudio (single-header) -------------------------------------------
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

# ---- tinyobjloader (single-header) ---------------------------------------
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# --------------------------------------------------------------------------
# Game executable
# --------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

add_executable(HackathonGame ${GAME_SOURCES})

target_include_directories(HackathonGame PRIVATE src)

target_link_libraries(HackathonGame PRIVATE
    glfw
    glad_gl_core_33
    glm::glm
    stb
    miniaudio
    tinyobjloader
)

if(MSVC)
    target_compile_options(HackathonGame PRIVATE /W4 /permissive-)
else()
    target_compile_options(HackathonGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy assets next to the binary so relative paths work in build/run dirs.
add_custom_command(TARGET HackathonGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:HackathonGame>/assets
)
