# Exe patches (runtime, via `dinput8.dll`)

Proxy `dinput8.dll` next to `H5_Game.exe` forwards the DirectInput exports to the system DLL and patches the game in memory at load.

## Safety model

1. **Build gate**: PE `TimeDateStamp` + `SizeOfImage` must match a known build. For SteamStub exes, `SizeOfImage` is first unwrapped (see Steam below). Unknown build -> nothing patched, log `unknown build`.
2. **Byte check**: every site is compared against its expected original bytes, with 32-bit absolute operands rebased for ASLR, before anything is written. On mismatch that feature is skipped and logged (`unexpected bytes, skipped`).
3. **Dependencies**: base/HoF widescreen needs the own pak (layout data). Without the pak, widescreen is skipped and the HoF frame and cursor fixes still apply.
4. Log: `homm5-widescreen.log` next to the exe, or `%LOCALAPPDATA%\homm5-widescreen\homm5-widescreen-<build>.log` when the game folder is read-only (the exes are `asInvoker`, so there is no VirtualStore redirection).
5. Options: `homm5-widescreen.ini` next to the exe, `[options] skip_fade=0|1`.

Patch primitives: a call or jump to DLL code NOP-padded to the original instruction length, plain NOPs, and tables of single-instruction edits (original bytes, patched bytes, position of a rebased absolute operand). Use the exe's relocation table to find which bytes of an instruction are absolute addresses.

## Heroes V 1.6 (base)

### Widescreen virtual screen

- Inside the virtual-screen setter, a short block copies the design size (1024/768) into the virtual size and then loads the log stream. That block is replaced by a call to DLL code that writes ToE's rule (see [engine.md -> Virtual screen](engine.md#virtual-screen)) into the virtual globals instead.
- The stub must leave the registers the setter uses afterwards exactly as the original block did: height register unchanged, an SSE register holding `(float)width`, and the log-stream pointer reloaded. Registers that are dead there can be clobbered.
- Everything else (screen alignment, re-anchored HUD, fullscreen option) is data: base already has ToE's layout engine.

### Own pak and fade

- Own pak: see [Own pak](#own-pak-base--hof). Base's VFS factory takes one stack argument (`ret 4`); folder `datals`.
- Fade: two calls NOPed, see [intro-and-fade.md](intro-and-fade.md).

## Hammers of Fate 2.1

HoF needs more - its UI layer does not have the virtual-screen and screen-alignment code paths that base and ToE have.

### 1. Window frame

- HoF creates a `WS_POPUP` window even when windowed, so there is no title bar or border.
- The single `call [SetWindowPos]` that sizes the main window is redirected to DLL code. When `D3DPRESENT_PARAMETERS.Windowed` is set, it sets style `WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`, applies `SWP_FRAMECHANGED`, then grows `cx`/`cy` by the frame size so the **client** area equals the backbuffer. Otherwise style `WS_POPUP | WS_VISIBLE | WS_SYSMENU`. Then it calls the real `SetWindowPos`.

### 2. Cursor offset (depends on 1)

- `NUI::CCursor` maps the mouse through `GetWindowRect`, which includes the new frame, so the drawn cursor sat below the hit point.
- Its two `call [GetWindowRect]` sites are redirected to a helper returning the client rect in screen coordinates (`GetClientRect` + `ClientToScreen`).

### 3. Virtual screen

- The screen-size setter (a function that stores `(float)w`/`(float)h`, `ret 8`) is replaced by a jump to a `__stdcall` function that stores the same values and also writes ToE's virtual size into the virtual-size globals that vanilla never writes.
- **Operand edits** in the coordinate-conversion cluster:
  - `mulss x, [1/1024]` -> `divss x, [virtual_w]`
  - `divss x, [768]` -> `divss x, [virtual_h]`
  - `movss x, [1024]` / `[768]` -> `movss x, [virtual_w]` / `[virtual_h]`
  - two `mulss xmm0, xmm3` -> `divss xmm0, xmm3`, because `xmm3` is now loaded with virtual_w instead of 1/1024
  - Only operands change. The constants are shared with unrelated code.
- **Font y-scale**: the glyph scaler sets y-scale = x-scale \* (4/3) / (screen W/H) unless a markup flag is set, so at `16:9` all text looked vertically squashed. Base 1.6 always uses y = x. Fix: NOP the conditional jump (`je` -> 2-byte NOP) so the `1:1` branch always runs. Neutral at `4:3`.

### 4. Root-screen alignment

- The root-screen layout function hard-codes every root rect to (0, 0, 1024, 768) with four `mov dword [esp+...], imm` instructions. They are replaced by a call to DLL code that computes the rect.
- HoF has no ToE `HorAlign`/`VerAlign`. Instead, the root screen's own Placement alignment stands in for it (`SWindowScreen` DB reached through the window object; `VerAllign` at DB+0x94, `HorAllign` at DB+0x9C). Vanilla ignores these fields for roots.
  - 0 `EPA_LOW_END` and 1 `ERA_CENTER` -> center the 1024x768 box in the virtual screen
  - 2 `EPA_HIGH_END` -> right/bottom
  - 3 `EPA_MARGIN` -> whole virtual screen
- The HoF recipe writes these values into the root screens (see [layout-recipes.md](layout-recipes.md#hof-21)).
- The instruction after the site is a virtual call through `eax` with `ecx` = this, so the stub preserves both.

### 5. Minimap

- `NUI::CAdvMinimap` builds its draw rects and click offsets from the **DB** Placement position instead of the laid-out position. Once its parent moved, the frame moved but the picture stayed, causing minimap contents appear displaced.
- Memory operands that read the DB position are retargeted to the window's laid-out position fields.
- Other widgets that may render from DB positions too (untested): `CTownMinimap`, 3D portraits, the RMG minimap.

### Own pak and fade

- Own pak: HoF's VFS factory takes register arguments only; folder `dataa1`.
- Fade: two calls NOPed.

## Tribes of the East 3.1

Widescreen is native. Only the optional fade NOPs. The own pak needs no hook: it sits in `data\` and wins by entry date.

## Heroes 5.5 (`MMH55.exe`, `MMH55_64.exe`)

- Has own executables: the 2011 Ubisoft 3.1 build + `55.dll`.
- `MMH55_64.exe` is the same file with the large-address-aware flag set.
- Only the fade NOPs, at this build's own two call sites.
- 5.5's own widescreen/windowed/borderless features are runtime patches from `55.dll`. This patch touches no window, D3D or cursor code on ToE, so the two don't collide.

## Own pak (base / HoF)

- Goal: mount `<game root>\<datals|dataa1>\homm5-widescreen.pak` above every shipped pak without taking a shipped file name.
- The exe has exactly one call to its `CWinVFS` factory, which scans `data\*.pak` and mounts the hard-coded table. The DLL replaces that call with a stub: call the original factory, then call the exe's own wildcard mount function `__thiscall (CWinVFS*, const GameString* pattern, int priority)` with our path at **priority 100** (the shipped maximum is 4 in base and 3 in HoF). The game's string type is `{begin, end, capacity}` char pointers.
- Text-pak entries (priority -1) still compare by date, so the pak's entries carry recent dates.
- ToE needs no hook: just put the pak in `data\` with newer entry dates.

## Steam (SteamStub 3.1)

- The Steam exes are the GOG builds wrapped in SteamStub 3.1 x86. They have the same timestamp and the same sections, but `.text` is AES-256-CBC encrypted on disk, the entry point sits in an extra trailing `.bind` section, and SizeOfImage grows by `0x2E000`. Decrypted `.text` is byte-identical to GOG, so no per-store tables are needed.
- Unwrapped SizeOfImage = the `.bind` section's VirtualAddress (= the GOG value).
- `DINPUT8.dll` is still imported, so the proxy's `DllMain` runs **before** the stub decrypts `.text`. Fix: when a `.bind` section is present, hook the IAT slot `api-ms-win-crt-runtime-l1-1-0.dll!_initterm_e` once. It is the first import the CRT calls in `__scrt_common_main_seh`, after decryption and before any C/C++ initializer. Restore the slot, install the patches, then forward.
- The stub restores the full code (stolen bytes included). A runtime dump matched the GOG `.text` plus relocations with 0 differing bytes.
- Steam app ids: base = 15170, HoF = 15380 (HoF installs `bin\H5_Game.exe` => base 1.6 exe whose stub hands off to app 15170; not a runnable base install), ToE = 15370. Steam HoF launches `bina1\testapp.exe`, a byte-identical copy of `bina1\H5_Game.exe`, so the same `dinput8.dll` covers it. No VAC on any of them.

## Not supported

- **Dark Messiah mod** (`binDM\H5_Game.exe`, shipped with ToE). It is a separate build whose UI stretches, unlike ToE 3.1. Support would need its own hook sites. Presumably it's an early in development demo built from code older than release ToE.
- **Retail DVD builds**: expecting per-region DRM (StarForce, SecuROM). Code virtualization can make individual sites unportable.
