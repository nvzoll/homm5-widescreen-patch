# HoMM5 Widescreen Patch - technical notes

Knowledge needed to reproduce or extend the patch (with coding agents in mind).

| File | Contents |
|---|---|
| [engine.md](engine.md) | Archives and priorities, xdb quirks, virtual screen, profiles, saves |
| [exe-patches.md](exe-patches.md) | `dinput8.dll`: build gate, hooks |
| [layout-recipes.md](layout-recipes.md) | Widescreen UI data for base and HoF: porting rules, exceptions, recipe format |
| [fonts.md](fonts.md) | Font file format, the Palatino generator, the `Brief` face, restyled texts |
| [intro-and-fade.md](intro-and-fade.md) | Splash fade removal, intro redirect, the blank Theora clip |
| [re-hints.md](re-hints.md) | Reverse-engineering workflow and traps |

## Supported builds

| Game | Exe | PE TimeDateStamp | SizeOfImage (GOG) |
|---|---|---|---|
| Heroes V 1.6 | `bin\H5_Game.exe` | `6847E45E` | `F83000` |
| Hammers of Fate 2.1 | `bina1\H5_Game.exe` | `6847E484` | `F62000` |
| Tribes of the East 3.1 | `bin\H5_Game.exe` | `6841A475` | `F05000` |
| Heroes 5.5 mod | `bin\MMH55.exe` | `4EA07667` (2011 Ubisoft 3.1 build) | `DE9000` |

The GOG exes are 2025 rebuilds with ASLR on, so every patch must be rebased at runtime, including 32-bit absolute operands inside instructions.

Steam exes are the GOG builds wrapped in SteamStub 3.1. Decrypted code is byte-identical to GOG, so every patch applies to both stores.

Folders each game reads:

| Game | Data folders |
|---|---|
| Base 1.6 | `data\*.pak` + hard-coded table in `datals\` + `UserMODs\*.h5u` |
| HoF 2.1 | `data\*.pak` + hard-coded table in `dataa1\` |
| ToE 3.1 | `data\*.pak` + `UserMODs\*.h5u`/`*.zip` |

## Summary

ToE 3.1 already does widescreen: a virtual screen with h=768 + (width follows the aspect ratio) + per-screen alignment and re-anchor HUD widgets in the UI data.

Base and HoF share most of that UI code but compute a fixed 1024x768 virtual screen, and their data lacks ToE's anchors.

Our patch

- hooks the virtual-screen computation in the exe to use ToE's rule + some HoF-only fixes the engine is missing.

- ports ToE's layout data onto the user's own base/HoF UI files as diffs ("recipes") applied at install time. Recipes are line diffs against game files, only a handful of small ToE-only XML files are embedded whole.
