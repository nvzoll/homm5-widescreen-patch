#include "pak_install.h"

#include "data/data_build.h"
#include "data/intro_skip.h"
#include "product.h"

#include <windows.h>

#include <cstdint>
#include <fstream>
#include <span>

namespace {

std::span<const uint8_t> Resource(const wchar_t* name) {
    const HRSRC resource = FindResourceW(nullptr, name, RT_RCDATA);
    if (!resource) return {};
    const HGLOBAL loaded = LoadResource(nullptr, resource);
    return {static_cast<const uint8_t*>(LockResource(loaded)), SizeofResource(nullptr, resource)};
}

std::span<const uint8_t> EmbeddedRecipe(GameId game) {
    switch (game) {
        case GameId::Base: return Resource(L"BASE16_RECIPE");
        case GameId::HammersOfFate: return Resource(L"HOF21_RECIPE");
        case GameId::TribesOfTheEast: break;
    }
    return {};
}

std::filesystem::path IniPath(const Game& game) {
    return game.exe.parent_path() / L"homm5-widescreen.ini";
}

std::filesystem::path VideoPath(const Game& game, const char* name) {
    return game.root / L"video" / name;
}

bool WriteBytes(const std::filesystem::path& path, std::span<const uint8_t> bytes, std::wstring& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (out.close(), out.fail()) {
        error = path.wstring() + L": cannot write";
        return false;
    }
    return true;
}

bool DeleteIfPresent(const std::filesystem::path& path, std::wstring& error) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return true;
    error = path.wstring() + L": cannot delete";
    return false;
}

bool InstallIntroFiles(const Game& game, std::wstring& error) {
    const auto clip = Resource(L"BLANK_OGG");
    if (clip.empty()) {
        error = L"the embedded intro clip is missing from this build.";
        return false;
    }
    const std::string playlist = IntroPlaylistXml();
    return WriteBytes(VideoPath(game, kIntroClip), clip, error) &&
           WriteBytes(VideoPath(game, kIntroPlaylist), {reinterpret_cast<const uint8_t*>(playlist.data()), playlist.size()}, error);
}

bool UninstallIntroFiles(const Game& game, std::wstring& error) {
    return DeleteIfPresent(VideoPath(game, kIntroClip), error) && DeleteIfPresent(VideoPath(game, kIntroPlaylist), error);
}

}  // namespace

std::filesystem::path GamePakPath(const Game& game) {
    const wchar_t* folder = L"data";
    if (game.id == GameId::Base) folder = L"datals";
    if (game.id == GameId::HammersOfFate) folder = L"dataa1";
    return game.root / folder / L"" OWN_PAK_NAME;
}

bool ReadFontsOption(const Game& game) {
    return GetPrivateProfileIntW(L"options", L"fonts", 1, IniPath(game).c_str()) != 0;
}

bool InstallGamePak(const Game& game, bool fonts, bool skipIntro, std::wstring& error) {
    const auto recipe = EmbeddedRecipe(game.id);
    if (game.id != GameId::TribesOfTheEast && recipe.empty()) {
        error = L"the embedded layout recipe is missing from this build.";
        return false;
    }
    const GamePakOptions options{fonts, skipIntro};
    const bool built = recipe.empty() && !fonts && !skipIntro ? DeleteIfPresent(GamePakPath(game), error)
                                                              : BuildGamePak(game.id, game.root, recipe, options, GamePakPath(game), error);
    if (!built) return false;
    if (!(skipIntro ? InstallIntroFiles(game, error) : UninstallIntroFiles(game, error))) return false;
    if (!WritePrivateProfileStringW(L"options", L"fonts", fonts ? L"1" : L"0", IniPath(game).c_str())) {
        error = IniPath(game).wstring() + L": cannot write";
        return false;
    }
    return true;
}

bool UninstallGamePak(const Game& game, std::wstring& error) {
    return DeleteIfPresent(GamePakPath(game), error) && UninstallIntroFiles(game, error);
}
