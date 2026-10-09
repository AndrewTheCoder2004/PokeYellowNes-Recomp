# Thunder Emperor Recomp (Lei Dian Huang – Bi Ka Qiu Chuan Shuo)

Offline universal recompiler project for the unlicensed Chinese NES game
"The Thunder Emperor: The Legend of Yellow Mouse" (iNES mapper 163, 2 MB PRG, CHR-RAM),
inspired by `bryanthaboi/gen1recomp`.

## Architecture & Project Layout

```
.
├── core/
│   ├── include/thunder_core.h     # Portable C99 core (6502 CPU, PPU, Mapper 163, RAM/VRAM)
│   └── src/thunder_core.c         # Zero-dependency execution engine and scanline rasterizer
├── platforms/
│   ├── sdl2/main.c                # Desktop (Win10/11, Linux, BSD, macOS, Haiku) & mobile frontend

│   ├── devkitpro/Makefile         # Switch, 3DS, Wii, NDS (devkitA64, devkitARM, devkitPPC)
│   ├── playstation/Makefile       # PS Vita (VitaSDK), PSP (pspdev), PS2 (ps2dev), PS1
│   ├── dreamcast/Makefile         # Sega Dreamcast (KallistiOS)
│   ├── xbox_og/Makefile           # Original Xbox (nxdk)
│   └── platforms.json             # Exhaustive 35-platform matrix & input mapping definitions
├── config/
│   └── input_map.json             # Unified input bindings: Gamepad, Keyboard, Touch
├── mods/
│   └── example_palette/mod.json   # Mod loader specification (IPS patches & runtime byte diffs)
└── tools/
    ├── thunder.py                 # Offline CLI launcher, asset extractor, and mod patcher
    └── generate_web.py            # Universal web/mobile bundle generator
```

## ROM Requirement
Place your legitimate dump named:
`Lei Dian Huang - Bi Ka Qiu Chuan Shuo (China)(Unlicensed).nes`
in the project root. (SHA-1: `88ccfb00e1a18ae3c1711eb88d43628e26ea7897`).
The ROM and generated `userdata/` remain git-ignored to comply with repository requirements.

## Features
- Mapper 163 Support: Full register banking (`$5000`–`$53FF`) and Waixing/Nanjing protection registers (`$5101`, `$5501`).
- Unified Input Mapping: `config/input_map.json` routes physical controllers (XInput/DirectInput/SDL), keyboard, and on-screen multi-touch buttons across all supported architectures.
- Mod Engine: Supports hot-applying IPS binary patches and JSON-specified byte injections into output ROM images.

## CLI & Launcher Commands
```bash
python3 tools/thunder.py info                 # Validate ROM, header, mapper, and vectors
python3 tools/thunder.py extract --png        # Extract PRG banks & 2bpp tile sheets to userdata/assets
python3 tools/thunder.py mods                 # List all detected mods in ./mods
python3 tools/thunder.py build --mods <id>    # Apply mods & produce userdata/patched.nes
python3 tools/thunder.py input --class pc     # Show active input mappings (pc, mobile, console)
python3 tools/thunder.py platforms            # Output full platform support matrix
python3 tools/generate_web.py                 # Build standalone offline web player
```

## Building Native Targets
- Desktop (Win10/11, Linux, macOS, BSD, Haiku):
  ```bash
  cmake -B build && cmake --build build
  ./build/thunder_sdl
  ```
- Nintendo Consoles (Switch, 3DS, Wii, NDS):
  ```bash
  cd platforms/devkitpro && make TARGET=switch
  ```
- PlayStation (PS Vita, PSP, PS2):
  ```bash
  cd platforms/playstation && make TARGET=vita
  ```
- Dreamcast:
  ```bash
  cd platforms/dreamcast && make
  ```
- Xbox (OG):
  ```bash
  cd platforms/xbox_og && make
  ```
Made With SI.
