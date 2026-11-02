# Engine facts

## Archives (.pak)

- A `.pak` is a plain zip. The VFS is `NVFS::CWinVFS` / `CCombinerVFS`; lookups are case-insensitive.
- Mount order: the `CWinVFS` constructor scans `data\*.pak` at priority 0. The scan follows `FindFirstFile` order, which on NTFS is the upper-cased name order. Base and HoF then mount a hard-coded table.
- **Replace rule** (same in all three games): when two archives contain the same entry, then
  - if either priority is -1 or the priorities are equal, the **newer zip entry date** wins;
  - otherwise the higher priority wins.
- Base table (folder `datals\`): `p5-texts` -1, `p5-data` 1, `p6-texts` -1, `p6-data` 2, `p7-texts` -1, `p7-data` 3, `p8-texts` -1, `p8-data` 4. `p7`/`p8` are not shipped. In one mode the same function also mounts `Maps/*.h5m`, `DuelPresets/*.h5p`, `UserCampaigns/*.h5c`, `UserMODs/*.h5u` at -1.
- HoF table (folder `dataa1\`): `texts` -1, `p0..p3-data` 1, `p0..p3-texts` -1, `a1-texts`/`a1-sound`/`a1-data` 2, `a1p1-data`/`a1p1-sound` 3, `a1p1-texts` -1.
- ToE has no table: everything is priority 0 and dates decide. A `zz_*.pak` in `data\` with newer entry dates overrides anything.
- Consequence for modders: to beat base `p6-data` (priority 2) you need priority >= 3. `zz_*.pak` (priority 0) loses. `datals\p7-data.pak` works but the name may collide with future releases.
- **Never reuse a shipped pak name.** `dataa1\a1p1-sound.pak` is free in the English GOG package but shipped in Russian; GOG Galaxy deletes or replaces files with shipped names on language switch or verify.
- The patch's own pak is mounted by the DLL at priority 100 (see [exe-patches.md -> Own pak](exe-patches.md#own-pak-base--hof)).
- Text paks (priority -1) are resolved by date, so the newest entry wins. Font templates therefore come from whichever text pak is newest per game. Do not assume `p6-texts`. In the English GOG package the effective templates are base `data\p3-texts.pak`, HoF `dataa1\a1-texts.pak`, ToE `data\texts.pak`.
- ToE `data.pak` has 84,312 entries but a 16-bit EOCD count (18,776). A zip reader that trusts the EOCD count misses entries; iterate the central directory to its end instead. The repo's minizip is patched to do that.
- The game builds its root with `GetCurrentDirectoryA`, so the game itself breaks on paths outside the ANSI code page.

## xdb and UI data

- UI is described by XML `.xdb` files under `UI/`. The type is in the file name, e.g. `MainMenu.(WindowScreen).xdb`. References look like `href="Foo.(WindowSimple).xdb#xpointer(/WindowSimple)"`.
- **`types.xml` is a schema gate.** The DB loader silently drops fields that `types.xml` does not declare. ToE declares `HorAlign`/`VerAlign` on `WindowScreen` (enum `Align`, type id `37077649`, chunk ids 5/6) only in `a2p1-data.pak`'s `types.xml`. The base exe knows these fields, but base data never declares them, so adding them to screens alone does nothing.
- **Placement override flag.** In `<Placement>` fields (`Position`, `Size`, `VerAllign`, `HorAllign`, margins) each value is a `<First>`/`<Second>` pair, and `<Second>` means "field is set". With `<Second>false</Second>` the engine ignores `<First>` and falls back to the shared xdb or the default. Changing an anchor needs `<Second>true</Second>` too.
- Placement anchors: `EPA_LOW_END`, `EPA_HIGH_END`, `EPA_MARGIN` (stretch), `ERA_CENTER`. With `ERA_CENTER` the x/y position is the offset of the window's center from the parent's center. With `EPA_HIGH_END` it is the offset from the right/bottom edge.
- Root-screen alignment enum `Align` (ToE): 0 `ALIGN_LOW_END`, 1 `ALIGN_CENTER`, 2 `ALIGN_HIGN_END` (sic), 3 `ALIGN_SCREEN`. The child-placement switch uses the same numbering 0..3.
- **Saves keep their own copies of UI DB objects** ("UI uses copy of DB-objects via CPtr" in `types.xml`). Layout changes show only in a new game or mission, not in a loaded save.
- Root-screen constructor default: HorAlign = 1 (center), VerAlign = 0.
- Markup tags: `UI/Consts/mlTags/*.txt` (UTF-16LE with BOM), mapped by `UI/uiconsts.(UIGameConstsH5).xdb`. Example chain on the loading screen: `UI/pwlscreen/MapDesc.txt` -> `<pwl_h3>` -> `<h3>` -> `<font face=Header size=22>`.
- In markup, `size=N` without a unit is a point size, scaled with resolution. An attribute `forcefontsize` exists.
- Game options are items in `uiconsts`: `full_screen` (`gfx_fullscreen`) exists in all three games but is `<Hide>true</Hide>` in base/HoF.

## Virtual screen

- UI coordinates live in a virtual screen mapped onto the backbuffer. Design size is 1024x768.
- In base and ToE a setter logs `Screen resolution changed: aspect = ..., virtual screen resolution = ...` (a good string to search for) and then runs the console command `reposition_interfaces`. It runs only on a resolution change, not every frame.
- ToE rule: `a = W/H`; if `a <= 4/3`: vW = 1024, vH = int(1024/a + 0.99); else vH = 768, vW = int(768*a + 0.99).
- Base copies the design size into the virtual size, so the UI always stretches. The 14 coordinate-conversion functions after the setter are identical to ToE apart from addresses.
- HoF has the same conversion cluster, but it reads float constants 1024, 768 and 1/1024 from `.rdata` instead of virtual globals. Its virtual-size getter reads a pair of globals that hold 1024/768 and are never written. **The constants are shared with non-UI code**, so redirect operands inside the cluster and never patch the constants.
- HoF has no `reposition_interfaces` and no screen-level `HorAlign`. Every root screen gets a hard-coded rect (0, 0, 1024, 768).

## ToE-only features the base exe lacks

- `WidenPictureIfPossible` (combat arena).
- `MaintainAspectRatio` on `SWindowPlayer` (video player).

Base/HoF data cannot enable these; they would need exe work.

## Window and display

- Base `NGfx::SetMode` reads `gfx_fullscreen` and `gfx_resolution` and restyles the window itself. Console command `toggle_fullscreen` exists.
- Base `CGameOptionsScreen` already handles `gfx_fullscreen` the same way ToE does. A fullscreen checkbox needs data only.
- HoF creates its main window as `WS_POPUP` even in windowed mode, never restyles it, and sizes it with a single `SetWindowPos`. `D3DPRESENT_PARAMETERS.Windowed` tells which mode is active.
- HoF options apply: group `video` runs `gfx_recreate`, which ends in SetMode and re-reads both variables, so a live switch works.
- HoF's software cursor `NUI::CCursor` stores `GetCursorPos - GetWindowRect`. That is only correct for a frameless window.

## Profiles

- Real profiles live in `Documents\My Games\<window title>\Profiles\` (`SHGetFolderPath(CSIDL_PERSONAL)`, fallback `Profiles\`). `profiles\default_profile\` in the game folder holds only templates copied on first profile creation.
- Base: `My Games\Heroes of Might and Magic V\Profiles\<name>\user.cfg`. HoF: `...\Hammers of Fate\Profiles\<name>\user_a1.cfg`.
- Startup runs `..\Profiles\autoexec.cfg`. Its last line is the console command `mainmenu`.
- Heroes 5.5 writes into ToE's own profile. Its `Saves\Campaign.sav` crashes vanilla ToE 3.1 when a campaign mission starts (the campaign name getter returns null). A new profile fixes it. Check profile history before treating a ToE crash as a patch bug.

## Language packages

- Exes are identical across languages. Pak sets differ per language: for example, Russian lacks `data\texts.pak` but ships a real `dataa1\a1p1-sound.pak`.
- Layout xdbs are language-independent: the same recipes rebuild byte-identical output on English and Russian data, and on Steam data.
- Font metrics differ per language (Russian tables hold 94 Cyrillic glyphs, English 0) and even between base and HoF of the same language. Build fonts per game from that game's own templates.
- Simplified Chinese is a different package (no `datals\`, widechar font format, `widechar.chk` markers). The atlas generator does not handle it.
