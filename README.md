# Game Boy Color Emulator

> Prebuilt native and web artifacts are already packaged, so you can jump straight to `prebuilt-web/`, `build/GBC`, or visit the hosted emulator at https://kevinhuai.com/gameboy-emulator with some very legal ROMS (don't tell Nintendo).

A Game Boy Color emulator written in C++ with SDL3 support for both native and web (Emscripten) builds.

## Requirements
- C++ compiler with C++23 support
- CMake
- SDL3

## Native Build

### Dependencies
- C++ compiler with C++23 support
- CMake
- SDL3

On macOS install SDL3 with `brew install sdl3`; on Linux install `libsdl3-dev` (or similar), and on Windows download the latest release from https://github.com/libsdl-org/SDL/releases.

### Building
```bash
git clone https://github.com/KevinH15291/Game-Boy-Emulator
cd Game-Boy-Emulator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Running
```bash
./build/GBC path/to/rom.gb
```
`GBC` will prompt for the ROM path if none is provided.

## Web Build

### Prerequisites
- Emscripten (see https://emscripten.org/docs/getting_started/downloads.html)

### Building
```bash
emcmake cmake -S . -B build_wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build_wasm --config Release
```

The resulting `GBC.js`/`GBC.wasm` pair will appear under `build_wasm/` and can be mirrored into `prebuilt-web/` for hosting. Open `prebuilt-web/index.html` in a modern browser or serve the directory with `python3 -m http.server`.

> **Note:** Prebuilt web assets (`prebuilt-web/`) are already included so you can skip the build step if you just want the js/wasm. The emulator is currently hosted at https://kevinhuai.com/gameboy-emulator if you would like to use it for playing games.

## Controls
A: A  
B: S  
Start: Z  
Select: X

## Screenshots

![image](https://github.com/user-attachments/assets/7fe80fb9-3d9b-4772-95e2-ff6a724c9a89)

![image](https://github.com/user-attachments/assets/df287824-9dcd-4e75-878e-453c6f37ebd3)

![image](https://github.com/user-attachments/assets/06c53c0a-8065-4ae2-84d5-715ba7d5ead6)
