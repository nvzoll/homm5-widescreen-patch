// Dev tool: homm5-font-build <out dir> <base|hof|toe> <game root>
//           homm5-font-build <out dir> paks <pak>...   (text priority, in order)
// Writes body.pak, brief.pak, pwl.pak for byte-for-byte checks against the Python builds.
#include "../src/data/font_build.h"
#include "../src/data/game_paks.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

constexpr uint32_t kDosDateTime = (2026 - 1980) << 25 | 10 << 21 | 20 << 16 | 12 << 11;

using Builder = bool (*)(const PakArchives&, std::vector<PakFile>&, std::wstring&);

bool Write(const std::filesystem::path& out, Builder build, const PakArchives& game, std::wstring& error) {
    std::vector<PakFile> files;
    if (!build(game, files, error) || !WritePak(out, files, kDosDateTime, error)) return false;
    std::fwprintf(stdout, L"wrote %ls (%zu entries)\n", out.c_str(), files.size());
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) {
        std::fwprintf(stderr, L"usage: homm5-font-build <out dir> <base|hof|toe> <game root>\n"
                              L"       homm5-font-build <out dir> paks <pak>...\n");
        return 2;
    }
    const std::filesystem::path outDir = argv[1];
    const std::wstring mode = argv[2];
    PakArchives game;
    std::wstring error;
    bool ok = true;
    if (mode == L"paks") {
        for (int i = 3; i < argc && ok; ++i) ok = game.Add(argv[i], PakArchives::kTextPriority, error);
    } else if (mode == L"base" || mode == L"hof" || mode == L"toe") {
        const GameId id = mode == L"base" ? GameId::Base : mode == L"hof" ? GameId::HammersOfFate : GameId::TribesOfTheEast;
        ok = argc == 4 && MountGamePaks(id, argv[3], game, error);
    } else {
        std::fwprintf(stderr, L"unknown mode %ls\n", argv[2]);
        return 2;
    }
    ok = ok && Write(outDir / L"body.pak", BuildBodyFonts, game, error) && Write(outDir / L"brief.pak", BuildBriefFonts, game, error) &&
         Write(outDir / L"pwl.pak", BuildRestyledTexts, game, error);
    if (!ok) {
        std::fwprintf(stderr, L"failed: %ls\n", error.c_str());
        return 1;
    }
    return 0;
}
