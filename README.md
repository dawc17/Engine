# daw's Voxel Engine

welcome to my small minecraft clone (maybe something more in the future) made in C++ with OpenGL!

one day a few years ago i had this very specific itch that i never quite got to scratching, so recently i decided it was finally time. this is the result!

<img width="1919" height="994" alt="image" src="https://github.com/user-attachments/assets/e0aed9a9-35a7-43ec-b5aa-10199390482a" />

## technical details
- **chunk streaming**: streams a configurable radius of chunks around the player with async load/save through a multi‑threaded job system.
- **procedural terrain**: fbm‑style terrain with stone/dirt/grass/sand strata, sea level water fill, and tree decorator.
- **procedural caves**: minecraft‑inspired caves (cheese + spaghetti) carved underground.
- **greedy meshing**: merges adjacent faces with the same texture to reduce draw calls.
- **texture array pipeline**: block textures are loaded from individual files in `assets/textures/` into a `GL_TEXTURE_2D_ARRAY` with mipmaps + anisotropic filtering.
- **biome tinting**: grass/leaves tint is applied dynamically in shaders, with per-pixel masking so only the intended parts are tinted.
- **water system**: source + flowing levels with edge search, falling water, and optional caustics.
- **particles**: instanced quads for block break particles.
- **item icons**: inventory block icons are rendered from 3D block models (offscreen) instead of static images.
- **survival system**: health + hunger bars, fall damage, drowning, regen, and respawn.
- **player controller**: aabb collision, gravity, jumping, noclip.
- **block interaction**: raycast selection with wireframe highlight, lmb break / rmb place.
- **day/night cycle**: dynamic sky, fog, and ambient lighting.

## prerequisites

- CMake 3.14+
- C++17 compiler and OpenGL 4.6 capable GPU/drivers
- GLFW development package (or enable `-DGLFW_FETCH=ON`; defaults to ON on Windows, OFF elsewhere)
- Linux packages (Debian/Ubuntu):  
`sudo apt install/yay -S build-essential cmake libglfw3-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev`

## building on linux

```bash
cmake -B build -S . [-DGLFW_FETCH=ON]
cmake --build build       # produces build/src/VoxelEngine
```

pass `-DGLFW_FETCH=ON` if you want cmake to fetch/build glfw (useful on windows or when a system package is unavailable).

### building on windows

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DGLFW_FETCH=ON .
cmake --build build --config Release
build\src\Release\VoxelEngine.exe
```

If you already have GLFW installed, set `-DGLFW_FETCH=OFF` and ensure CMake can find `glfw3`.

## running

```bash
./build/src/VoxelEngine
```

- Release builds on Windows land at `build\src\Release\VoxelEngine.exe`; Debug builds live in `build\src\Debug\VoxelEngine.exe`.
- Shaders and assets are copied to the build output directory at build time.

## distribution

the `build/src/release/` folder is self-contained and ready to distribute.

zip the `release/` folder and share it. saves are created in a `saves/` folder next to the executable.

## controls

| Key           | Action                                 |
| ------------- | -------------------------------------- |
| `W/A/S/D`     | Move player                            |
| `Mouse`       | Look around (cursor locked by default) |
| `Space`       | Jump (or move up in noclip)            |
| `Left Shift`  | Move down (noclip only)                |
| `Mouse Wheel` | Cycle placeable blocks                 |
| `LMB`         | Break block                            |
| `RMB`         | Place block                            |
| `U`           | Toggle cursor lock                     |
| `Tab`         | Toggle debug menu                      |
| `T`           | Open chat input                        |
| `R`           | Respawn when dead                      |
| `Esc`         | Close chat (if open) or exit           |

## chat commands

- `/noclip` — toggle noclip
- `/time set <0..1>` or `/time day|noon|sunset|night|sunrise`
- `/gamemode survival|creative|0|1`
- `/seed` — print terrain seed

## debug menu

to be able to access the debug menu, run the executable with the `--debug` argument in the terminal.

the imgui debug window provides:

- fps, player position/velocity/orientation
- chunk coordinates and selection info
- wireframe mode toggle
- noclip mode toggle
- async loading toggle
- move speed slider
- max fps limiter
- day/night controls (auto time, time of day, day length, fog density)
- water controls (simulation tick rate, caustics)
- fun modes (drunk / disco / earthquake)

## troubleshooting

- **texture/shader not found**: run from the repo root or `build/` so relative paths resolve; ensure `src/shaders/` and `assets/` are copied next to the executable.
- **glfw link errors**: install the glfw dev package or enable `-DGLFW_FETCH=ON`.
- **caves breaking through surface**: adjust `surfaceMargin` in `CaveGenerator.h`.
- **performance issues**: reduce render distance or enable async loading in the debug menu.

