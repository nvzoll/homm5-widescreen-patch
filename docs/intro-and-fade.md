# Splash fade and intro videos

## Splash fade

- The splash (`<game root>\splash.bmp`) is a plain GDI popup that only BitBlts on `WM_PAINT`.
- For the fade effect, at startup (only when no debugger is attached) the game starts a thread that creates `ScreenFadeWnd`, a maximized black topmost layered window whose alpha is ramped by a 10 ms timer. The main thread then `Sleep`s for 2280 ms. A second call after `NGfx::SetMode` stops the fade.
- Patch: NOP both calls (start and stop), 5 bytes each. Same pattern in base, HoF, ToE and Heroes 5.5's exe; only the call sites differ. To find them, search for the `ScreenFadeWnd` class name and follow its creator back to the startup function.

## Intro videos

What does not work:
- Editing or emptying `video\intro.xml` (`<Movies/>`) skips the videos, but leaves the main menu without textures and breaks world-map cloud shadows. Some global graphics state is only set up when the video player actually plays something.
- Starting with `mainmenu 1` in `autoexec.cfg` (the path used when returning to the menu from a game) breaks textures the same way.
- Editing shipped files is fragile anyway: GOG Galaxy restores them on every verify or language switch.

What the patch does:
- The movie name -> playlist mapping is game data: `UI/UIGameRoot.(UIGameRoot).xdb`, item `<ID>intro</ID>` with `<MovieDesc>../video/intro.xml</MovieDesc>` (HoF/ToE: `intro_a1.xml`; the exes only contain `"intro"`).
- With "skip intro", the own pak carries the engine-resolved `UIGameRoot` with only that `MovieDesc` value changed, to `../video/homm5-intro.xml`. The patcher also installs `video\homm5-intro.xml`, a one-item playlist (skippable) that plays `../video/homm5-blank`, and `video\homm5-blank.ogg`. No exe hook is needed, and shipped files stay untouched.
- Heroes 5.5 replaces `UIGameRoot` in its own pak. The patch then carries a copy of the mod's version, so a mod update needs a reinstall of the patch.

## The blank clip

Have to be precise with dummy clip. Simple stub with modern ffmpeg won't do.

- **Exact codecs.** HoMM5 video player is built on libtheora 1.0alpha3 (2004) and libvorbis 1.0.1. Theora decoder only accepts the original VP3 quantisation tables. Anything from a modern libtheora, ffmpeg included, writes the newer tables, so the headers are rejected. The audio thread is already running at that point and crashes. Had to encode with exact same library versions.
- **Dummy file is too small.** The player reads the file in 4 KB chunks. After the headers it only takes further pages out of its buffer after another successful read. A clip that is read completely during header parsing leaves its last audio page in the buffer. Game shows a black screen until you click left mouse button. For that matter, the crafted clip ends with a 4 KB padding page in a third logical stream, which the video player ignores, so at least one more read happens.
