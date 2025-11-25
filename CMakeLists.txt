cmake_minimum_required(VERSION 3.14)
project(VoxelEngine)

set(CMAKE_CXX_STANDARD 17)

# GLM
set(GLM_ENABLE_CXX_17 ON CACHE BOOL "Build GLM with C++17 support" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use header-only GLM" FORCE)
add_subdirectory(libs/glm-1.0.2)

add_subdirectory(src)
