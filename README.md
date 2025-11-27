# Engine

Voxel chunk playground built with modern OpenGL 4.6. Renders greedy-meshed 16x16x16 chunks using a 2D texture array sliced from `assets/textures/blocks.png` (32x32 tiles) and overlays ImGui debug stats. GLAD, ImGui, and GLM are vendored under `libs/`. Since the last README update the engine picked up a full player controller, block palette, multi-layer chunk streaming, terrain gen with trees, and basic skylight.
<img width="2559" height="1370" alt="image" src="https://github.com/user-attachments/assets/83c4f447-f5d9-4f24-9652-7366e7e1eb76" />


## Features
- Streams a 5x5x5 column of chunks around the player, rebuilds greedy-meshed VBO/EBOs on demand, and uploads sky-lit vertices.
- Procedural FBM terrain with stone/dirt/grass/sand strata plus tree decorator that places log + leaf volumes pulled from the 32x32 tile atlas.
- Texture atlas loader turns `assets/textures/blocks.png` into a 2D texture array with mipmaps and full-anisotropy sampling.
- Player entity with AABB collisions, gravity, jumping, noclip toggle, and a configurable move speed; centered crosshair aids aiming.
- Raycast selection outline drives a block palette (dirt, grass, stone, sand, oak log, oak leaves); LMB breaks blocks, RMB places the currently selected block while avoiding the player volume.
- ImGui debug window shows FPS, camera position/orientation, velocity, chunk coords, currently hovered block, block palette selection, wireframe toggle, noclip toggle, and move-speed slider.

## Prerequisites
- CMake 3.14+
- C++17 compiler and OpenGL 4.6 capable GPU/drivers
- GLFW development package (or enable `-DGLFW_FETCH=ON` to fetch/build it; defaults to ON on Windows, OFF elsewhere)
- Linux packages (Debian/Ubuntu): `sudo apt install build-essential cmake libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev`

## Build
```bash
cmake -B build -S . [-DGLFW_FETCH=ON]
cmake --build build       # produces build/src/VoxelEngine
```
Pass `-DGLFW_FETCH=ON` if you want CMake to fetch/build GLFW (useful on Windows or when a system package is unavailable).

### Windows (VS2022 Build Tools)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DGLFW_FETCH=ON .
cmake --build build --config Release
build\src\Release\VoxelEngine.exe
```
If you already have GLFW installed, set `-DGLFW_FETCH=OFF` and ensure CMake can find `glfw3`.

## Run
```bash
./build/src/VoxelEngine
```
- Release builds on Windows land at `build\src\Release\VoxelEngine.exe`; Debug builds live in `build\src\Debug\VoxelEngine.exe`.
- Shaders and assets are automatically copied to the build output directory.

## Distribution
The `build/src/Release/` folder is self-contained and ready to distribute:
```
Release/
├── VoxelEngine.exe
├── shaders/
│   ├── default.vert
│   ├── default.frag
│   ├── selection.vert
│   └── selection.frag
└── assets/
    └── textures/
        └── blocks.png
```
Zip up the `Release/` folder and share it. Saves will be created in a `saves/` folder next to the executable.

## Controls
- `W/A/S/D`: walk/fly the player depending on noclip state
- Mouse: look around (cursor locked by default)
- `Space`: jump (or move up while noclip is enabled)
- `Left Shift`: move down while noclip is enabled
- Mouse wheel: cycle the placeable block palette (dirt, grass, stone, sand, oak log, oak leaves)
- `LMB`: break the targeted block
- `RMB`: place the selected block on the hit face (skips if it would intersect the player)
- `U`: toggle cursor lock/unlock (re-centers on lock)
- `Esc`: close the window
- ImGui debug window: wireframe toggle, noclip toggle, camera speed slider, FPS/camera/chunk/block readouts

## Project Layout
- `src/main.cpp`: main loop, chunk streaming, texture array setup, ImGui wiring, raycast selection
- `src/Chunk*`, `Meshing.*`, `BlockTypes.*`: chunk storage, greedy meshing, block/atlas mapping
- `src/Player.*`, `Raycast.*`: player physics (AABB, gravity, noclip) plus block hit testing and placement helpers
- `src/ShaderClass.*`, `VBO.*`, `VAO.*`, `EBO.*`: OpenGL helpers
- `src/selection.vert/.frag`: thin shader used for the wireframe highlight
- `assets/textures/`: atlas (`blocks.png`) and reference textures

## Troubleshooting
- Texture/shader not found: run from the repo root or `build/` so relative paths resolve; ensure `assets/textures/blocks.png` and `src/default.vert/.frag` exist.
- GLFW link errors: install the GLFW dev package (or enable `-DGLFW_FETCH=ON` to build it locally).
