#include "data_build.h"

#include "pak.h"
#include "recipe.h"

#include <array>
#include <vector>

namespace {

struct SourcePak {
    const wchar_t* path;
    int priority;
};

// The paks the game itself would read the recipe's source entries from, with its priorities
// (base FUN_006c4a60, HoF FUN_006ec930; unlisted paks mount at 0). Must match tools/gen_recipe.py.
constexpr std::array kBaseSources = {
    SourcePak{L"datals/p6-data.pak", 2},
    SourcePak{L"datals/p5-data.pak", 1},
    SourcePak{L"data/p3c-data.pak", 0},
    SourcePak{L"data/data.pak", 0},
};
constexpr std::array kHofSources = {
    SourcePak{L"dataa1/a1p1-data.pak", 3},
    SourcePak{L"dataa1/a1-data.pak", 2},
    SourcePak{L"data/p3c-data.pak", 0},
    SourcePak{L"data/data.pak", 0},
};

std::span<const SourcePak> SourcesFor(GameId game) {
    switch (game) {
        case GameId::Base: return kBaseSources;
        case GameId::HammersOfFate: return kHofSources;
        case GameId::TribesOfTheEast: break;
    }
    return {};
}

}  // namespace

bool BuildDataPak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe,
                  const std::filesystem::path& out, std::wstring& error) {
    const auto sources = SourcesFor(game);
    if (sources.empty()) {
        error = L"no data pak is needed for this game";
        return false;
    }
    PakArchives archives;
    for (const SourcePak& source : sources) {
        if (!archives.Add(root / source.path, source.priority, error)) return false;
    }
    std::vector<PakFile> files;
    uint32_t dosDateTime = 0;
    return ApplyRecipe(recipe, archives, files, dosDateTime, error) && WritePak(out, files, dosDateTime, error);
}
