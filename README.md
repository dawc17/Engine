# VoxelEngine

Voxel chunk playground built with modern OpenGL 4.6. Renders greedy-meshed 16x16x16 chunks using a 2D texture array sliced from `assets/textures/blocks.png` (32x32 tiles) and overlays ImGui debug stats. GLAD, ImGui, and GLM are vendored under `libs/`. Features a full player controller, block palette with HUD icons, multi-layer chunk streaming, procedural terrain with trees, cave generation (cheese caves + spaghetti tunnels), dynamic day/night cycle with fog, and world persistence.

<img width="2559" height="1387" alt="image" src="https://github.com/user-attachments/assets/1c89a0ac-84b4-4e1b-8ad4-b8de7c35f59a" />

<img width="2559" height="1365" alt="image" src="https://github.com/user-attachments/assets/e32e9d99-66db-4261-8e2c-b1228f1df731" />

<img width="2559" height="1386" alt="image" src="https://github.com/user-attachments/assets/7da748df-7503-4248-abf5-09700a6acd37" />

## Note about AI Usage
This readme was generated using AI. AI was also used for debugging when the developer was in absolute despair and could not for his life
figure out what the hell an euler angle was or how exactly physics work. I hope you understand. Other than that, the code is 80% human effort.

## Features
- **Chunk Streaming**: Streams a configurable radius of chunks around the player with async loading/saving via a multi-threaded job system.
- **Procedural Terrain**: FBM-based terrain generation with stone/dirt/grass/sand strata plus tree decorator that places log + leaf volumes.
- **Cave Generation**: Minecraft-inspired cave system combining "cheese caves" (large open caverns) with "spaghetti caves" (winding tunnels) that stay underground.
- **Day/Night Cycle**: Dynamic sky color, sun brightness, fog, and ambient lighting that smoothly transitions through dawn, day, dusk, and night.
- **Greedy Meshing**: Efficient mesh generation that combines adjacent faces with the same texture into larger quads, reducing draw calls.
- **Texture Atlas**: Loads `assets/textures/blocks.png` into a 2D texture array with mipmaps and anisotropic filtering.
- **Block Palette**: 8 placeable block types (dirt, grass, stone, sand, oak log, oak leaves, glass, oak planks) with HUD icons displayed in the top-right corner.
- **Player Physics**: AABB collision detection, gravity, jumping, and noclip mode for debugging.
- **Block Interaction**: Raycast-based block selection with wireframe highlight; LMB breaks blocks, RMB places blocks (with player intersection check).
- **World Persistence**: Chunks and player position are saved to disk using a region-based file format with zlib compression.
- **Fun Modes**: Drunk mode (camera wobble), disco mode (rainbow sky), and earthquake mode (screen shake) for chaos.

## Prerequisites
- CMake 3.14+
- C++17 compiler and OpenGL 4.6 capable GPU/drivers
- GLFW development package (or enable `-DGLFW_FETCH=ON` to fetch/build it; defaults to ON on Windows, OFF elsewhere)
- Linux packages (Debian/Ubuntu): `sudo apt install/yay -S build-essential cmake libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev`

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
        ├── blocks.png
        └── hud_blocks/
            └── *.png
```
Zip up the `Release/` folder and share it. Saves will be created in a `saves/` folder next to the executable.

## Controls
| Key | Action |
|-----|--------|
| `W/A/S/D` | Move player (walk or fly in noclip mode) |
| `Mouse` | Look around (cursor locked by default) |
| `Space` | Jump (or move up in noclip mode) |
| `Left Shift` | Move down (noclip mode only) |
| `Mouse Wheel` | Cycle through placeable blocks |
| `LMB` | Break targeted block |
| `RMB` | Place selected block on hit face |
| `U` | Toggle cursor lock/unlock |
| `Tab` | Toggle debug menu visibility |
| `Esc` | Close the window |

## Debug Menu
The ImGui debug window provides:
- FPS counter and player position/velocity/orientation
- Chunk coordinates and selected block info
- Wireframe mode toggle
- Noclip mode toggle
- Async loading toggle
- Move speed slider
- Max FPS limiter
- Day/night cycle controls (auto time, time of day, day length, fog density)
- Fun modes: Drunk Mode, Disco Mode, Earthquake Mode
- Block texture randomizer

## Project Layout
```
src/
├── main.cpp              # Main loop, rendering, input handling
├── Chunk.*, ChunkManager.*   # Chunk storage and management
├── Meshing.*             # Greedy meshing algorithm
├── BlockTypes.*          # Block definitions and atlas mapping
├── TerrainGenerator.*    # Procedural terrain generation
├── CaveGenerator.*       # Cave system (cheese + spaghetti caves)
├── Player.*, Camera.*    # Player physics and camera
├── Raycast.*             # Block raycasting for selection/placement
├── JobSystem.*           # Multi-threaded async chunk loading
├── RegionManager.*       # World save/load with zlib compression
├── ShaderClass.*, VBO.*, VAO.*, EBO.*  # OpenGL helpers
└── *.vert, *.frag        # GLSL shaders
```

## Troubleshooting
- **Texture/shader not found**: Run from the repo root or `build/` so relative paths resolve; ensure `assets/textures/blocks.png` exists.
- **GLFW link errors**: Install the GLFW dev package or enable `-DGLFW_FETCH=ON`.
- **Caves breaking through surface**: Adjust `surfaceMargin` in `CaveGenerator.h` to keep caves deeper underground.
- **Performance issues**: Reduce chunk load radius or enable async loading in the debug menu.
