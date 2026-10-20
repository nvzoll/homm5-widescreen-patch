# HoMM5 Widescreen Patch

This mod adds proper windowed mode to HoMM5/Hammers of Fate and realigns UI without stretching on 16:9 resolutions. Also, for all three games it optionally removes fade-to-black effect on launch + removes intro videos. It also optionally replaces Monotype Corsiva in regular text (non-headers) with Palatino Linotype.

Currently only the [GOG version](https://www.gog.com/en/game/heroes_of_might_and_magic_5_bundle) is supported.

## Implementation details

### The blank intro clip ([src/data/blank.ogg](src/data/blank.ogg))

Trivia found on google suggests removing intro videos by editing intro.xml. That works, but causes no-texture main menu and some textures in game like cloud shadows to break. To overcome that, I generated 0.4s blank clip. It was not straightforward:

- **Exact codecs.** HoMM5 video player is built on libtheora 1.0alpha3 (2004) and libvorbis 1.0.1. Theora decoder only accepts the original VP3 quantisation tables. Anything from a modern libtheora, ffmpeg included, writes the newer tables, so the headers are rejected. The audio thread is already running at that point and crashes. Had to encode with exact same library versions.
- **Dummy file is too small.** The player reads the file in 4 KB chunks. After the headers it only takes further pages out of its buffer after another successful read. A clip that is read completely during header parsing leaves its last audio page in the buffer. Game shows a black screen until you click left mouse button. For that matter, the crafted clip ends with a 4 KB padding page in a third logical stream, which the video player ignores, so at least one more read happens.

## Credits

Built by Opus 5.5, tested by me.

This mod was made possible by [REA: Reverse Engineer Anything](https://github.com/morluto/rea) skill set.
