#include "data_build.h"

#include "font_build.h"
#include "game_paks.h"
#include "intro_skip.h"
#include "pak.h"
#include "recipe.h"

#include <algorithm>
#include <cwctype>
#include <set>
#include <vector>

namespace {

// Newer than every shipped entry, which matters wherever the game compares dates (text paks, ToE).
constexpr uint32_t kGamePakDosDateTime = (2026 - 1980) << 25 | 10 << 21 | 20 << 16 | 12 << 11;

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
    return text;
}

bool CheckUnique(const std::vector<PakFile>& files, std::wstring& error) {
    std::set<std::string> names;
    for (const PakFile& file : files) {
        if (!names.insert(Lower(file.name)).second) {
            error = std::wstring(file.name.begin(), file.name.end()) + L": built twice";
            return false;
        }
    }
    return true;
}

}  // namespace

bool BuildDataPak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe,
                  const std::filesystem::path& out, std::wstring& error) {
    PakArchives archives;
    std::vector<PakFile> files;
    uint32_t dosDateTime = 0;
    return MountGamePaks(game, root, archives, error) && ApplyRecipe(recipe, archives, files, dosDateTime, error) &&
           WritePak(out, files, dosDateTime, error);
}

bool BuildGamePak(GameId game, const std::filesystem::path& root, std::span<const uint8_t> recipe, const GamePakOptions& options,
                  const std::filesystem::path& out, std::wstring& error) {
    PakArchives archives;
    if (!MountGamePaks(game, root, archives, error)) return false;
    std::vector<PakFile> files;
    uint32_t recipeDate = 0;
    if (!ApplyRecipe(recipe, archives, files, recipeDate, error)) return false;
    if (options.fonts && !(BuildBodyFonts(archives, files, error) && BuildBriefFonts(archives, files, error) &&
                           BuildRestyledTexts(archives, files, error))) {
        return false;
    }
    if (options.skipIntro && !BuildIntroSkip(archives, files, error)) return false;
    if (files.empty()) {
        error = L"nothing to install";
        return false;
    }
    if (!CheckUnique(files, error)) return false;
    auto temporary = out;
    temporary += L".tmp";
    if (!WritePak(temporary, files, kGamePakDosDateTime, error)) return false;
    std::error_code ec;
    std::filesystem::rename(temporary, out, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        error = out.wstring() + L": cannot replace";
        return false;
    }
    return true;
}
