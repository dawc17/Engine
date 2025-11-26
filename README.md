# Engine

Voxel chunk playground built with modern OpenGL 4.6. Renders greedy-meshed 16x16x16 chunks using a 2D texture array sliced from `assets/textures/blocks.png` (32x32 tiles) and overlays ImGui debug stats. GLAD, ImGui, and GLM are vendored under `libs/`.
<img width="2559" height="1370" alt="image" src="https://github.com/user-attachments/assets/83c4f447-f5d9-4f24-9652-7366e7e1eb76" />

## Features
- Streams chunks around the camera and uploads meshes on demand (greedy meshing + element/index buffers).
- Texture atlas loader turns `assets/textures/blocks.png` into a 32x32 layer array with mipmaps and anisotropic filtering.
- FPS-style camera (`WASD` + mouse look) with runtime speed slider and cursor lock toggle.
- Raycast selection outlines targeted blocks; `LMB` breaks blocks, `RMB` places a stone block on the hit face (avoids placing inside the player).
- ImGui debug window shows FPS, camera position/orientation, current chunk, selected block id/distance, and a wireframe toggle.

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
- Running from the repo root or `build/` works for both shaders and the atlas thanks to `SHADER_DIR` and the relative texture lookup in `src/main.cpp`.
- Release builds on Windows land at `build\src\Release\VoxelEngine.exe`; Debug builds live in `build\src\Debug\VoxelEngine.exe`.

## Controls
- `W/A/S/D`: fly the camera
- Mouse: look around (cursor locked by default)
- `U`: toggle cursor lock/unlock (re-centers on lock)
- `LMB`: break the targeted block
- `RMB`: place a stone block on the face you hit (skips if it would intersect the player)
- `Esc`: close the window
- ImGui debug window: wireframe toggle, camera speed slider, FPS/camera/chunk/block readouts

## Project Layout
- `src/main.cpp`: main loop, chunk streaming, texture array setup, ImGui wiring, raycast selection
- `src/Chunk*`, `Meshing.*`, `BlockTypes.*`: chunk storage, greedy meshing, block/atlas mapping
- `src/ShaderClass.*`, `VBO.*`, `VAO.*`, `EBO.*`: OpenGL helpers
- `assets/textures/`: atlas (`blocks.png`) and reference textures

## Troubleshooting
- Texture/shader not found: run from the repo root or `build/` so relative paths resolve; ensure `assets/textures/blocks.png` and `src/default.vert/.frag` exist.
- GLFW link errors: install the GLFW dev package (or enable `-DGLFW_FETCH=ON` to build it locally).
