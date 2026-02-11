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
    world/Biome.cpp
    world/CaveGenerator.cpp
    world/WaterSimulator.cpp
    rendering/ParticleSystem.cpp
    rendering/ItemModelGenerator.cpp
    rendering/ToolModelGenerator.cpp
    ui/DebugUI.cpp
    ui/HUD.cpp
    core/WorldSession.cpp
    core/Renderer.cpp
    rendering/Frustum.cpp
)

include(${CMAKE_SOURCE_DIR}/cmake/embed_resources.cmake)

set(_EMBED_H "#pragma once\n\n")
set(_EMBED_CPP "#include \"embedded_assets.h\"\n\n")

set(_TEX "${CMAKE_SOURCE_DIR}/assets/textures")
set(_SHD "${CMAKE_SOURCE_DIR}/src/shaders")

embed_asset(embed_dirt_png                  "${_TEX}/dirt.png")
embed_asset(embed_grass_top_png             "${_TEX}/grass_top.png")
embed_asset(embed_grass_side_snowed_png     "${_TEX}/grass_side_snowed.png")
embed_asset(embed_stone_png                 "${_TEX}/stone.png")
embed_asset(embed_sand_png                  "${_TEX}/sand.png")
embed_asset(embed_log_oak_png               "${_TEX}/log_oak.png")
embed_asset(embed_log_oak_top_png           "${_TEX}/log_oak_top.png")
embed_asset(embed_leaves_oak_png            "${_TEX}/leaves_oak.png")
embed_asset(embed_glass_png                 "${_TEX}/glass.png")
embed_asset(embed_planks_oak_png            "${_TEX}/planks_oak.png")
embed_asset(embed_cobblestone_png           "${_TEX}/cobblestone.png")
embed_asset(embed_log_spruce_png            "${_TEX}/log_spruce.png")
embed_asset(embed_log_spruce_top_png        "${_TEX}/log_spruce_top.png")
embed_asset(embed_leaves_spruce_png         "${_TEX}/leaves_spruce.png")
embed_asset(embed_planks_spruce_png         "${_TEX}/planks_spruce.png")
embed_asset(embed_snow_png                  "${_TEX}/snow.png")
embed_asset(embed_grass_side_overlay_png    "${_TEX}/grass_side_overlay.png")
embed_asset(embed_diamond_pickaxe_png       "${_TEX}/tools/diamond_pickaxe.png")

embed_asset(embed_destroy_stage_0_png       "${_TEX}/destroy/destroy_stage_0.png")
embed_asset(embed_destroy_stage_1_png       "${_TEX}/destroy/destroy_stage_1.png")
embed_asset(embed_destroy_stage_2_png       "${_TEX}/destroy/destroy_stage_2.png")
embed_asset(embed_destroy_stage_3_png       "${_TEX}/destroy/destroy_stage_3.png")
embed_asset(embed_destroy_stage_4_png       "${_TEX}/destroy/destroy_stage_4.png")
embed_asset(embed_destroy_stage_5_png       "${_TEX}/destroy/destroy_stage_5.png")
embed_asset(embed_destroy_stage_6_png       "${_TEX}/destroy/destroy_stage_6.png")
embed_asset(embed_destroy_stage_7_png       "${_TEX}/destroy/destroy_stage_7.png")
embed_asset(embed_destroy_stage_8_png       "${_TEX}/destroy/destroy_stage_8.png")
embed_asset(embed_destroy_stage_9_png       "${_TEX}/destroy/destroy_stage_9.png")

embed_asset(embed_default_vert              "${_SHD}/default.vert")
embed_asset(embed_default_frag              "${_SHD}/default.frag")
embed_asset(embed_selection_vert            "${_SHD}/selection.vert")
embed_asset(embed_selection_frag            "${_SHD}/selection.frag")
embed_asset(embed_destroy_vert              "${_SHD}/destroy.vert")
embed_asset(embed_destroy_frag              "${_SHD}/destroy.frag")
embed_asset(embed_water_vert                "${_SHD}/water.vert")
embed_asset(embed_water_frag                "${_SHD}/water.frag")
embed_asset(embed_particle_vert             "${_SHD}/particle.vert")
embed_asset(embed_particle_frag             "${_SHD}/particle.frag")
embed_asset(embed_item_model_vert           "${_SHD}/item_model.vert")
embed_asset(embed_item_model_frag           "${_SHD}/item_model.frag")
embed_asset(embed_tool_model_vert           "${_SHD}/tool_model.vert")
embed_asset(embed_tool_model_frag           "${_SHD}/tool_model.frag")

set(EMBED_DIR "${CMAKE_BINARY_DIR}/generated")
embed_finalize("${EMBED_DIR}")

add_executable(VoxelEngine ${SOURCES} "${EMBED_DIR}/embedded_assets.cpp")
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

target_include_directories(VoxelEngine PRIVATE "${CMAKE_BINARY_DIR}/generated")

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
