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
    audio/AudioEngine.cpp
    audio/stb_vorbis_impl.c
)

include(${CMAKE_SOURCE_DIR}/cmake/embed_resources.cmake)

set(_EMBED_H "#pragma once\n\n")
set(_EMBED_CPP "#include \"embedded_assets.h\"\n\n")

set(_TEX "${CMAKE_SOURCE_DIR}/assets/textures")
set(_SHD "${CMAKE_SOURCE_DIR}/src/shaders")

# asset embedding

# tools
embed_asset(embed_diamond_pickaxe_png       "${_TEX}/tools/diamond_pickaxe.png")
embed_asset(embed_wood_pickaxe_png       "${_TEX}/tools/wood_pickaxe.png")
embed_asset(embed_stone_pickaxe_png       "${_TEX}/tools/stone_pickaxe.png")
embed_asset(embed_gold_pickaxe_png       "${_TEX}/tools/gold_pickaxe.png")
embed_asset(embed_iron_pickaxe_png       "${_TEX}/tools/iron_pickaxe.png")


# blocks
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

# misc
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

# shaders
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

# sounds
set(_SND "${CMAKE_SOURCE_DIR}/assets/sounds")

# footsteps
embed_asset(embed_footstep_grass1           "${_SND}/footsteps/grass/grass1.ogg")
embed_asset(embed_footstep_grass2           "${_SND}/footsteps/grass/grass2.ogg")
embed_asset(embed_footstep_grass3           "${_SND}/footsteps/grass/grass3.ogg")
embed_asset(embed_footstep_grass4           "${_SND}/footsteps/grass/grass4.ogg")
embed_asset(embed_footstep_grass5           "${_SND}/footsteps/grass/grass5.ogg")
embed_asset(embed_footstep_grass6           "${_SND}/footsteps/grass/grass6.ogg")
embed_asset(embed_footstep_stone1           "${_SND}/footsteps/stone/stone1.ogg")
embed_asset(embed_footstep_stone2           "${_SND}/footsteps/stone/stone2.ogg")
embed_asset(embed_footstep_stone3           "${_SND}/footsteps/stone/stone3.ogg")
embed_asset(embed_footstep_stone4           "${_SND}/footsteps/stone/stone4.ogg")
embed_asset(embed_footstep_stone5           "${_SND}/footsteps/stone/stone5.ogg")
embed_asset(embed_footstep_stone6           "${_SND}/footsteps/stone/stone6.ogg")
embed_asset(embed_footstep_wood1            "${_SND}/footsteps/wood/wood1.ogg")
embed_asset(embed_footstep_wood2            "${_SND}/footsteps/wood/wood2.ogg")
embed_asset(embed_footstep_wood3            "${_SND}/footsteps/wood/wood3.ogg")
embed_asset(embed_footstep_wood4            "${_SND}/footsteps/wood/wood4.ogg")
embed_asset(embed_footstep_wood5            "${_SND}/footsteps/wood/wood5.ogg")
embed_asset(embed_footstep_wood6            "${_SND}/footsteps/wood/wood6.ogg")
embed_asset(embed_footstep_gravel1          "${_SND}/footsteps/gravel/gravel1.ogg")
embed_asset(embed_footstep_gravel2          "${_SND}/footsteps/gravel/gravel2.ogg")
embed_asset(embed_footstep_gravel3          "${_SND}/footsteps/gravel/gravel3.ogg")
embed_asset(embed_footstep_gravel4          "${_SND}/footsteps/gravel/gravel4.ogg")
embed_asset(embed_footstep_sand1            "${_SND}/footsteps/sand/sand1.ogg")
embed_asset(embed_footstep_sand2            "${_SND}/footsteps/sand/sand2.ogg")
embed_asset(embed_footstep_sand3            "${_SND}/footsteps/sand/sand3.ogg")
embed_asset(embed_footstep_sand4            "${_SND}/footsteps/sand/sand4.ogg")
embed_asset(embed_footstep_sand5            "${_SND}/footsteps/sand/sand5.ogg")
embed_asset(embed_footstep_snow1            "${_SND}/footsteps/snow/snow1.ogg")
embed_asset(embed_footstep_snow2            "${_SND}/footsteps/snow/snow2.ogg")
embed_asset(embed_footstep_snow3            "${_SND}/footsteps/snow/snow3.ogg")
embed_asset(embed_footstep_snow4            "${_SND}/footsteps/snow/snow4.ogg")
embed_asset(embed_footstep_cloth1           "${_SND}/footsteps/cloth/cloth1.ogg")
embed_asset(embed_footstep_cloth2           "${_SND}/footsteps/cloth/cloth2.ogg")
embed_asset(embed_footstep_cloth3           "${_SND}/footsteps/cloth/cloth3.ogg")
embed_asset(embed_footstep_cloth4           "${_SND}/footsteps/cloth/cloth4.ogg")
embed_asset(embed_footstep_ladder1          "${_SND}/footsteps/ladder/ladder1.ogg")
embed_asset(embed_footstep_ladder2          "${_SND}/footsteps/ladder/ladder2.ogg")
embed_asset(embed_footstep_ladder3          "${_SND}/footsteps/ladder/ladder3.ogg")
embed_asset(embed_footstep_ladder4          "${_SND}/footsteps/ladder/ladder4.ogg")
embed_asset(embed_footstep_ladder5          "${_SND}/footsteps/ladder/ladder5.ogg")

# block dig
embed_asset(embed_dig_grass1                "${_SND}/block_dig/grass/grass1.ogg")
embed_asset(embed_dig_grass2                "${_SND}/block_dig/grass/grass2.ogg")
embed_asset(embed_dig_grass3                "${_SND}/block_dig/grass/grass3.ogg")
embed_asset(embed_dig_grass4                "${_SND}/block_dig/grass/grass4.ogg")
embed_asset(embed_dig_stone1                "${_SND}/block_dig/stone/stone1.ogg")
embed_asset(embed_dig_stone2                "${_SND}/block_dig/stone/stone2.ogg")
embed_asset(embed_dig_stone3                "${_SND}/block_dig/stone/stone3.ogg")
embed_asset(embed_dig_stone4                "${_SND}/block_dig/stone/stone4.ogg")
embed_asset(embed_dig_wood1                 "${_SND}/block_dig/wood/wood1.ogg")
embed_asset(embed_dig_wood2                 "${_SND}/block_dig/wood/wood2.ogg")
embed_asset(embed_dig_wood3                 "${_SND}/block_dig/wood/wood3.ogg")
embed_asset(embed_dig_wood4                 "${_SND}/block_dig/wood/wood4.ogg")
embed_asset(embed_dig_gravel1               "${_SND}/block_dig/gravel/gravel1.ogg")
embed_asset(embed_dig_gravel2               "${_SND}/block_dig/gravel/gravel2.ogg")
embed_asset(embed_dig_gravel3               "${_SND}/block_dig/gravel/gravel3.ogg")
embed_asset(embed_dig_gravel4               "${_SND}/block_dig/gravel/gravel4.ogg")
embed_asset(embed_dig_sand1                 "${_SND}/block_dig/sand/sand1.ogg")
embed_asset(embed_dig_sand2                 "${_SND}/block_dig/sand/sand2.ogg")
embed_asset(embed_dig_sand3                 "${_SND}/block_dig/sand/sand3.ogg")
embed_asset(embed_dig_sand4                 "${_SND}/block_dig/sand/sand4.ogg")
embed_asset(embed_dig_snow1                 "${_SND}/block_dig/snow/snow1.ogg")
embed_asset(embed_dig_snow2                 "${_SND}/block_dig/snow/snow2.ogg")
embed_asset(embed_dig_snow3                 "${_SND}/block_dig/snow/snow3.ogg")
embed_asset(embed_dig_snow4                 "${_SND}/block_dig/snow/snow4.ogg")
embed_asset(embed_dig_cloth1                "${_SND}/block_dig/cloth/cloth1.ogg")
embed_asset(embed_dig_cloth2                "${_SND}/block_dig/cloth/cloth2.ogg")
embed_asset(embed_dig_cloth3                "${_SND}/block_dig/cloth/cloth3.ogg")
embed_asset(embed_dig_cloth4                "${_SND}/block_dig/cloth/cloth4.ogg")

# damage
embed_asset(embed_damage_fallbig            "${_SND}/damage/fallbig.ogg")
embed_asset(embed_damage_fallsmall          "${_SND}/damage/fallsmall.ogg")
embed_asset(embed_damage_hit1               "${_SND}/damage/hit1.ogg")
embed_asset(embed_damage_hit2               "${_SND}/damage/hit2.ogg")
embed_asset(embed_damage_hit3               "${_SND}/damage/hit3.ogg")

# liquid
embed_asset(embed_liquid_lava               "${_SND}/liquid/lava.ogg")
embed_asset(embed_liquid_lavapop            "${_SND}/liquid/lavapop.ogg")
embed_asset(embed_liquid_splash             "${_SND}/liquid/splash.ogg")
embed_asset(embed_liquid_splash2            "${_SND}/liquid/splash2.ogg")
embed_asset(embed_liquid_swim1              "${_SND}/liquid/swim1.ogg")
embed_asset(embed_liquid_swim2              "${_SND}/liquid/swim2.ogg")
embed_asset(embed_liquid_swim3              "${_SND}/liquid/swim3.ogg")
embed_asset(embed_liquid_swim4              "${_SND}/liquid/swim4.ogg")
embed_asset(embed_liquid_water              "${_SND}/liquid/water.ogg")

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
    target_link_libraries(VoxelEngine PRIVATE glad ${GLFW_TARGET} imgui opengl32 zlibstatic ole32 avrt)
    target_link_libraries(imgui PRIVATE ${GLFW_TARGET})
else()
    target_link_libraries(VoxelEngine PRIVATE glad ${GLFW_TARGET} GL imgui zlibstatic)
    target_link_libraries(imgui PRIVATE ${GLFW_TARGET})
endif()
