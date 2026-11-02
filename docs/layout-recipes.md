# Layout data and recipes

Base and HoF get ToE's widescreen layout by porting selected ToE 3.1 UI data onto the game's own UI files. Store as a recipe: line diffs plus a few whole new files. At install the patcher applies the recipe and writes the result into the own pak.

## Pipeline

1. **Resolve sources exactly like the engine.** Mount the game's paks in the engine's order with its priorities and replace rule (see [engine.md -> Archives](engine.md#archives-pak)). Take the winning copy of each entry. Do the same for ToE's `data\a2p1-data.pak` + `data\data.pak`. Getting this wrong => a file the game never loads.
2. **Build a baseline .pak.** A script applies the porting rules below, with ToE and base/HoF installed side by side, and writes every changed or added entry with one fixed entry date.
3. **Diff it into a recipe.** For each baseline entry: if the game has an entry with that name, store the source size, its CRC-32 and line hunks (`difflib.SequenceMatcher` over `splitlines(keepends=True)`, `autojunk=False`). Otherwise store the whole content. Check that applying the hunks reproduces the baseline entry exactly. 
4. **Apply at install** (`src/data/recipe.cpp`). A size or CRC mismatch aborts with "the game's copy differs".
5. **Verify** by rebuilding with the C++ applier (`homm5-data-build`) and comparing name, bytes and DOS date with the baseline pak. The same recipes reproduced byte-identical output on GOG and Steam data with varied languages.

### Recipe format (in this repo)

```
"H5RECIPE"            8 bytes
version   u32         = 1
dosDate   u32         entry date/time written into the output pak
count     u32
count * entry:
  nameLen u16, name   UTF-8 pak path, e.g. UI/MainMenu2/....xdb
  kind    u8          0 = NEW, 1 = PATCH
  NEW:    size u32, bytes
  PATCH:  srcSize u32, srcCrc32 u32, hunks u32,
          hunks * (offset u32, remove u32, insertLen u32, insert bytes)
          offsets are byte offsets into the source, ascending
```

## Base 1.6

Sources: base `datals\p6-data` (2), `datals\p5-data` (1), `data\p3c-data` (0), `data\data` (0). ToE: `a2p1-data`, `data`. Only `UI/**/*.xdb` that exist in both games are considered; `UI/UIEditor/` is skipped.

1. **Screen alignment.** ToE appends `<HorAlign>`/`<VerAlign>` (`ALIGN_SCREEN`, `ALIGN_CENTER`, `ALIGN_LOW_END`) at the end of 70+ root screen xdbs. These lines are added to the base files at the same position. 50+ of those files are otherwise byte-identical to base, four differ slightly (TemplateScreen, NecropolisTransformCreature, PWLScreen, Sphinx), and others are ToE-only screens.
2. **Re-anchoring.** For each window whose `<Placement>` blocks have the same count in both games but different `VerAllign`/`HorAllign` anchors, all `<Placement>` blocks are taken from ToE, giving 200+ windows. Without this the adventure HUD (jog dial, minimap, buttons) stays at the left at `16:9`. About 70+ more windows differ only in position or size; those are ToE design tweaks and were not ported.
3. **`types.xml`.** Base's `WindowScreen` type block is replaced with ToE's, and ToE's `Align` enum (`<__ServerPtr>37077649</__ServerPtr>`) is inserted. Without this, step 1 is dropped by the loader and every screen stays a centered 1024 box.
4. **Letterbox bars.** ToE's `UI/Common/BlackBorder/*` (6 files: black `BackgroundSimpleTexture` panels outside the centered 1024x768 box) are added, and their `<Item href=...BlackBorder...>` children are added to `CusceneScreen` (sic), `DialogueScreen`, `PuzzleMap` and `Video/VideoPlayer` (`.(WindowScreenShared)`). Those four files are otherwise identical to base.
5. **Options** (`uiconsts`): `full_screen` `Hide=false` and `OptionToDisableID=app_always_active`; `app_always_active` `Hide=false`; `gamma_correction` `DisableByCodeGlobalVar=~gfx_fullscreen`.
6. **Texts** for the "stay active" option: `UI/Consts/GameOptions/VideoAlwaysActive/{Name,Tooltip}.txt` from ToE `a2p1-texts`. They are English in every language.
7. **Town building name** (`UI/TownScreen/BuildingDesc.(WindowStaticText).xdb`): `<x>271</x>` -> `<x>15</x>` and `HorAllign` `EPA_LOW_END`/`false` -> `ERA_CENTER`/`true`. Because of the [override flag](engine.md#xdb-and-ui-data), changing only `<First>` moved the box to the left edge instead. ToE gets this fix too: its single-entry recipe.

## HoF 2.1

Sources: HoF `dataa1\a1p1-data` (3), `dataa1\a1-data` (2), `data\p3c-data` (0), `data\data` (0). Same candidate filter as base.

HoF cannot read ToE's `HorAlign`/`VerAlign`. The DLL's root-rect hook reads the root screen's own Placement anchors instead (see [exe-patches.md -> Root-screen alignment](exe-patches.md#4-root-screen-alignment)).

1. **Root alignment.** For each of the screens where ToE sets `HorAlign`/`VerAlign`, the HoF root `<Placement>` axis gets `ALIGN_SCREEN -> EPA_MARGIN`, `ALIGN_CENTER -> ERA_CENTER`, `ALIGN_LOW_END -> EPA_LOW_END`, `ALIGN_HIGN_END -> EPA_HIGH_END`, with `<Second>true</Second>`.
2. **Re-anchoring**, same rule as base: windows take ToE's Placement blocks. Exceptions:
   - **Corner-anchored** (ToE resized these panels for extra buttons): `GameOptions/MenuPanel`, `MainMenu2/SinglePlayer/MenuPanel`, `MPCreate/Menu`. They keep HoF's size and get HoF's own position re-expressed from the bottom-right corner of the 1024x768 design screen (`x += w - 1024`, `y += h - 768`, both axes `EPA_HIGH_END`).
   - **Kept as HoF**: `MPDMWait/Arenas.(WindowMSButton)`, `HireCreatures/HireToCaravanWindow.(WindowSimple)` (ToE changed them beyond anchors).
   - Files where the Placement block count differs are left alone, and the script lists them.
3. **Letterbox bars**: same as base, but each bordered screen is added only if it equals ToE's copy minus the border items.
4. **Loading screen background.** `PWLScreen.(WindowScreenShared)` takes ToE's `<Background>` (`pwlscreen/Back`, which ships in HoF `data.pak`). HoF's a1-data dropped it, so a stale frame showed outside the 1024 picture at `16:9`. Base 1.6 still has it.
5. **Adventure-map dialog bars** (`StartAdvMapDialog`, screen `UI/MiniDialogScreen/`). HoF borrows `DialogueScreen`'s fixed 1024-wide bars at x = 0. ToE gave the screen its own full-width ones. Taken from ToE: `Main.(WindowSimpleShared)`, `HeroText.(WindowTextView)`, `Panel.(WindowSimple|WindowSimpleShared|BackgroundSimpleScallingTexture)`, `Top.(WindowSimple|WindowSimpleShared|BackgroundSimpleScallingTexture)`. ToE's `Top` has no background, so `Top.(WindowSimpleShared)` gets `<Background href="Top.(BackgroundSimpleScallingTexture).xdb#..."/>`, and that texture's size is set to 1024x64, mirroring ToE's Panel. Visible e.g. on the HoF single mission "Iron Throne" (`Maps/SingleMissions/A1SM4`).
6. **Options**: `full_screen` `Hide=false`, `gamma_correction` `DisableByCodeGlobalVar=~gfx_fullscreen`. The video group's apply runs `gfx_recreate`, which re-reads `gfx_fullscreen`, so the switch is live.
7. **Town building name**: same edit as base.
8. Cutscenes (`UI/CutsceneScreen.(WindowScreen)`) stay as ported: centered + black borders.

## Adding a layout fix

- Find the entry the engine really loads (step 1 of the pipeline). Edit it, add it to the golden pak with the pak's fixed date, then regenerate the recipe. Keep the hunks minimal: they must apply to every language package, and so far layout xdbs have been language-independent.
- Remember the `<Second>` flag, and test in a **new** game, because saves carry their own UI copies.
- If a widget ignores its new anchor, it may render from its DB position like the HoF minimap did. That needs an exe fix, not data.

## ToE copied content

A few small XML files exist only in ToE and are embedded whole: BlackBorder files, MiniDialogScreen files (HoF) and option texts (base). Everything else in the recipes is line hunks.
