# Fonts

Replace Monotype Corsiva in body text (`Default_*`) with Palatino Linotype and adds a `Brief` face for the loading screen. Titles (`Header_*`) stay Corsiva. Everything is rendered during install with GDI from the Palatino Linotype that ships with Windows, using the game's own font files as templates, so each language's character set comes along.

## File format

Fonts are pre-rendered atlases.

- `UI/Fonts/<Name>_<size>.(Font).xdb`: uid, `<FaceName>`, texture reference.
- `UI/Fonts/<Name>_<size>.(Texture).dds` + `.(Texture).xdb`: A8R8G8B8, white glyphs, **RGB must equal alpha**. The texture xdb carries the atlas size.
- `bin/fonts/<uid>`: metrics.
  - 106-byte header: cell height at 0x0A, average width 0x0C, max width 0x0D, baseline 0x11.
  - 225 * `01 04 <u16 char>` (character table).
  - 225 * `02 38` + 7 * int32 `x1, y1, x2, y2, a, advance, width` (glyph boxes in the atlas).
  - 16-byte tail.
  - The shipped paks disagree on a junk dword repeated in the header (looks like a heap pointer). It is harmless.
- `UI/Fonts/FontsTable.(FontsTable).xdb` lists every face. It is byte-identical in base, HoF and ToE (English).

### Text layout

- Pen step = `(a + advance)` (+ pair kerning) * scale, in float, in all three exes. Corsiva stores a = left bearing, advance = the rest of the step.
- For point sizes: px = (int)(size * 4:3-width / 1024), then scale = px / atlas line height. Point sizes therefore follow resolution: `size=22` is about 41 px at 1440p and picks the `_40` atlas, while body `size=18` is about 34 px (`_32`).

## Generator

- Faces built: `Default_18, _20, _22, _26, _32, _40`.
- Palatino at **0.78x** the template size (matches Corsiva's x-height, about 13% wider), weight normal.
- Rendered by GDI at **4x** and box-downsampled (`(sum+2) >> 2` per 2x pass). GDI turns antialiasing off at small sizes via the font's `gasp` table, so a direct small render looks bad.
- Each glyph is padded 1 px with a 3 px gap in the atlas, which stops neighbouring glyphs bleeding in.
- Metrics: **advance = full GDI step + tracking - a**. An early version stored a = cell offset and advance = full step, so most glyphs came out 1 px short (about 10% at 18-22 px). Text looked cramped below 1440p and word gaps nearly vanished.
- Keep the template's character table: that is what keeps Cyrillic, Polish and other glyph sets intact.
- Check that Palatino is really installed: `CreateFont` + `GetTextFace` returns a substitute such as "Arial" when the face is missing.

## Mission brief (loading screen)

- Same as body text plus tracking 0.05 em, extra word spacing 0.06 em on U+0020/U+00A0 only, and glyphs 1.12x taller at the same width (GDI `lfWidth` = natural average width). Plain Palatino looked too tight and squashed on the loading screen.
- `Brief_22, _26, _32, _40`, uids = UUID5 of the name (fixed), appended to `FontsTable`.

## Restyled texts

| Change | Files |
|---|---|
| Tags overridden to Brief | `UI/Consts/mlTags/pwl_h3.txt` (`<font face=Brief size=22><color_white><outline>`), `pwl_h4.txt` (size 20, `<color_default>`) |
| Tag expansion inlined (`pwl_h3`, `pwl_h4`, `progress_bar` -> Brief) | `UI/pwlscreen/MapDesc.txt`, `MissionNumber.txt`, `ProgressBarText.txt`, `UI/AdventureScreen/EnemyTurnBox/player_name_text.txt` |
| `<justify>` -> `<left>` | `MapDesc.txt`, creature info description, hero biography, hero description format, save info map description, Tower of Astrologer description, town description, quest description format |
| Double spaces -> single | `pwlscreen/ChooseBonus_Text`, `Loading`, `PressAnyKey`, `WaitForPlayers`, the enemy-turn `player_name_text` |

Why inline as well as override:
- In base 1.6 an overridden tag file is served on the first loading screen but reverts to the original expansion on the second and later loads. HoF doesn't do this. The cause is somewhere in the tag cache and was never located. Inlining the expansion into the text sidesteps it.
- `progress_bar` also styles the adventure map's enemy-turn box, so the tag itself stays untouched and only its users get Brief inlined.
- Corsiva-era texts use double spaces for visual spacing, which looks wrong in Palatino.

Texts are UTF-16LE with BOM. Write them back the same way.

## Languages

- Built per game from that game's effective templates, resolved by the engine's pak rules (see [engine.md -> Archives](engine.md#archives-pak)), never shared across games or languages.
- Palatino Linotype covers Latin-1, Latin Extended-A (including the Hungarian double-acute o and u) and Cyrillic, so every GOG language except Simplified Chinese works. Chinese uses a different (widechar) font format and stays unsupported.
- Some translations may lack the loading-screen tag files. The text restyling then has nothing to change.
