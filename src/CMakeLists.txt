# === Sources for your app ===
set(SOURCES
    main.cpp
    VBO.cpp
    VAO.cpp
    EBO.cpp
    ShaderClass.cpp
    Camera.cpp
    Chunk.cpp
    ChunkManager.cpp
    Meshing.cpp
    BlockTypes.cpp
    Raycast.cpp
    Player.cpp
    RegionManager.cpp
    JobSystem.cpp
)

# === Create the executable FIRST ===
add_executable(VoxelEngine ${SOURCES})

# === ZLIB ===
include(FetchContent)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)
target_include_directories(VoxelEngine PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})

# === GLAD ===
set(GLAD_DIR ../libs/glad)
add_library(glad ${GLAD_DIR}/src/glad.c)
target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

# === IMGUI ===
set(IMGUI_DIR ../libs/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# === Include dirs for your executable ===
target_include_directories(VoxelEngine PUBLIC
    ../libs/glad/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(VoxelEngine PRIVATE SHADER_DIR=\"${CMAKE_SOURCE_DIR}/src\")

# === GLFW & OpenGL ===
if (WIN32)
    set(GLFW_FETCH_DEFAULT ON)
else()
    set(GLFW_FETCH_DEFAULT OFF)
endif()
option(GLFW_FETCH "Fetch/build GLFW locally instead of requiring a system package" ${GLFW_FETCH_DEFAULT})

if (GLFW_FETCH)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    set(GLFW_TARGET glfw)
else()
    find_package(glfw3 REQUIRED)
    if (TARGET glfw)
        set(GLFW_TARGET glfw)
    elseif (TARGET glfw3)
        set(GLFW_TARGET glfw3)
    elseif (TARGET glfw3::glfw)
        set(GLFW_TARGET glfw3::glfw)
    else()
        message(FATAL_ERROR "Could not find a GLFW target from find_package(glfw3)")
    endif()
endif()

target_link_libraries(VoxelEngine PRIVATE glm::glm)

if (WIN32)
    target_link_libraries(VoxelEngine PRIVATE glad ${GLFW_TARGET} imgui opengl32 zlibstatic)
    target_link_libraries(imgui PRIVATE ${GLFW_TARGET})
else()
    target_link_libraries(VoxelEngine PRIVATE glad ${GLFW_TARGET} GL imgui zlibstatic)
    target_link_libraries(imgui PRIVATE ${GLFW_TARGET})
endif()
