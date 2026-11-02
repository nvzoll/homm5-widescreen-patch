# Reversing hints

## Tooling

- **Ghidra** with a persistent project per exe. Analysis of a single H5_Game.exe takes ~15 min, so do it once and query the saved project headless (decompile, callers, xrefs, string search) instead of re-analyzing. Ghidra analysis targets work only on NTFS volumes.
- **RTTI** is present: vtable symbols (`NUI::CCursor`, `NUI::CAdvMinimap`, `CWindowScreen`, `NDb::SWindowScreen`, `NVFS::CWinVFS`, ...) are the fastest way into a subsystem.
- **Log strings**: the virtual-screen setter logs, the VFS has named console commands (`add search paths`), and `mainmenu`, `gfx_recreate`, `toggle_fullscreen` and `reposition_interfaces` are console commands with string-registered handlers.
- **ToE as a reference build.** ToE 3.1 implements what base/HoF lack (widescreen, screen alignment, fullscreen option). Diff the matching functions across builds: base and ToE share the virtual-screen and coordinate code nearly `1:1`, and HoF shares the conversion cluster with different operands.
- Use **live process reads**: read the virtual-size globals after a resolution change, scan the heap for `CWindowScreen` objects by vtable and dump their DB pointers, compare game cursor values against real input (`SetCursorPos` alone is ignored; send real `mouse_event` input). Frida works too.


## Traps

- **ASLR.** The 2025 GOG rebuilds have `DYNAMIC_BASE`. On-disk "code cave" without matching `.reloc` entries crashes. Runtime hooks must rebase absolute operands. Reloc table tells which instruction bytes are absolute addresses.
- **Shared constants.** HoF's UI math reads `.rdata` constants that unrelated code also uses. Patch operands, not constants.
- **Data that looks applied but isn't.** 1) The loader drops undeclared fields (`types.xml`). 2) `<Second>false</Second>` disables a Placement value. 3) Saves keep old UI copies. Check all three before suspecting code.
- **The setter is not polled.** Writing the screen size at runtime does nothing until a resolution change.
- **Pak resolution must match the engine exactly**, including the NTFS name order of the `data\*.pak` scan and the -1/date rule. Otherwise you diff against the wrong source file.
- **Profiles and 5.5.** Crashes after switching between Heroes 5.5 and vanilla ToE are most likely caused by 5.5's `Campaign.sav`.

## Verification

- Every generated artifact (layout paks, font paks) was first produced by a reference script and then reproduced byte for byte by the C++ patcher, comparing name, bytes and DOS date.
- Every exe hook first exists as a disk patch of a copy of the exe. The DLL then must produce the same live state (virtual size, cursor values, root-screen positions) before in-game testing.
- In-game checks: 4:3 and 16:9 (1080p, 1440p, 4K), windowed and fullscreen, main menu, adventure HUD, minimap clicks, town screen, combat, loading screen, HoF Iron Throne dialog, cutscenes. Always use a new game.
