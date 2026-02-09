set(SOURCES
    core/main.cpp
    ui/MainMenu.cpp
    core/MainGlobals.cpp
    gameplay/SurvivalSystem.cpp
    gameplay/Inventory.cpp
    rendering/opengl/VBO.cpp
    rendering/opengl/VAO.cpp
    rendering/opengl/EBO.cpp
    rendering/opengl/ShaderClass.cpp
    rendering/Camera.cpp
    world/Chunk.cpp
    world/ChunkManager.cpp
    rendering/Meshing.cpp
    utils/BlockTypes.cpp
    gameplay/Raycast.cpp
    gameplay/Player.cpp
    world/RegionManager.cpp
    utils/JobSystem.cpp
    world/TerrainGenerator.cpp
    world/CaveGenerator.cpp
    world/WaterSimulator.cpp
    rendering/ParticleSystem.cpp
    rendering/ItemModelGenerator.cpp
    rendering/ToolModelGenerator.cpp
)

add_executable(VoxelEngine ${SOURCES})
target_include_directories(VoxelEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/gameplay
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/opengl
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/world
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

set(SHADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/selection.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/selection.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/destroy.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/destroy.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/water.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/water.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/item_model.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/item_model.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tool_model.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tool_model.frag

)

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VoxelEngine>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILES} "$<TARGET_FILE_DIR:VoxelEngine>/shaders/"
    DEPENDS ${SHADER_FILES}
    COMMENT "Copying shader files"
)

add_custom_command(TARGET VoxelEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:VoxelEngine>/assets"
)

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

target_compile_definitions(VoxelEngine PRIVATE SHADER_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\")

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
