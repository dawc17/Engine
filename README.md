# Engine

Small OpenGL playground that draws a textured, rotating quad with ImGui controls. Uses CMake with vendored GLAD, ImGui, and GLM (see `libs/`).

## Features
- Indexed quad with texture sampling (assets/textures/container.jpg)
- ImGui overlay to toggle wireframe mode
- Simple shader/VAO/VBO/EBO helpers under `src/`

## Prerequisites
- CMake 3.14+
- C++17 compiler and OpenGL 4.6 capable GPU/drivers
- GLFW development package (or enable `-DGLFW_FETCH=ON` to fetch/build it)
- Linux packages (Debian/Ubuntu): `sudo apt install build-essential cmake libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev`

## Build
```bash
cmake -B build -S .
cmake --build build       # produces build/src/VoxelEngine
```
Pass `-DGLFW_FETCH=ON` if you want CMake to build GLFW locally (useful on Windows).

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
- Shaders are loaded from `src/default.vert` and `src/default.frag`. `SHADER_DIR` is baked into the binary, so running from the repo root or `build/` works.

## Controls
- ESC: close the window
- ImGui window: toggle wireframe mode

## Project Layout
- `src/MAIN.cpp`: main loop, texture setup, ImGui wiring
- `src/ShaderClass.*`: simple shader loader/activator
- `src/VBO.*`, `VAO.*`, `EBO.*`: buffer helpers
- `assets/textures/`: example texture assets

## Troubleshooting
- Shader file errors: confirm `src/default.vert` and `src/default.frag` exist and that `SHADER_DIR` still points to `src/`.
- GLFW link errors: install the GLFW dev package (or enable `-DGLFW_FETCH=ON` on Windows).
