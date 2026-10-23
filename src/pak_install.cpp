#include "pak_install.h"

#include "data/data_build.h"
#include "data/game_paks.h"
#include "data/intro_skip.h"
#include "product.h"

#include <windows.h>

#include <cstdint>
#include <cwchar>
#include <fstream>
#include <span>

namespace {

std::span<const uint8_t> Resource(const wchar_t* name) {
    const HRSRC resource = FindResourceW(nullptr, name, RT_RCDATA);
    if (!resource) {
        return {};
    }

    const HGLOBAL loaded = LoadResource(nullptr, resource);
    return {static_cast<const uint8_t*>(LockResource(loaded)), SizeofResource(nullptr, resource)};
}

std::span<const uint8_t> EmbeddedRecipe(GameId game) {
    switch (game) {
        case GameId::Base: return Resource(L"BASE16_RECIPE");
        case GameId::HammersOfFate: return Resource(L"HOF21_RECIPE");
        case GameId::TribesOfTheEast: return Resource(L"TOE31_RECIPE");
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
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
        return true;
    }

    error = path.wstring() + L": cannot delete";
    return false;
}

constexpr uint64_t FNV_OFFSET_BASIS = 0xCBF29CE484222325ull;
constexpr uint64_t FNV_PRIME = 0x100000001B3ull;
constexpr size_t FINGERPRINT_SIZE = 16 + 1;

void Mix(uint64_t& hash, const void* bytes, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ static_cast<const uint8_t*>(bytes)[i]) * FNV_PRIME;
    }
}

std::wstring GamePaksFingerprint(const Game& game) {
    uint64_t hash = FNV_OFFSET_BASIS;

    for (const GamePak& pak : GamePakFiles(game.id, game.root)) {
        WIN32_FILE_ATTRIBUTE_DATA info{};
        GetFileAttributesExW(pak.path.c_str(), GetFileExInfoStandard, &info);

        const std::wstring name = pak.path.parent_path().filename().wstring() + L"\\" + pak.path.filename().wstring();
        Mix(hash, name.data(), name.size() * sizeof(wchar_t));
        Mix(hash, &info.nFileSizeHigh, sizeof info.nFileSizeHigh);
        Mix(hash, &info.nFileSizeLow, sizeof info.nFileSizeLow);
        Mix(hash, &info.ftLastWriteTime, sizeof info.ftLastWriteTime);
    }

    wchar_t text[FINGERPRINT_SIZE]{};
    swprintf(text, FINGERPRINT_SIZE, L"%016llx", static_cast<unsigned long long>(hash));
    return text;
}

bool InstallIntroFiles(const Game& game, std::wstring& error) {
    const auto clip = Resource(L"BLANK_OGG");
    if (clip.empty()) {
        error = L"the embedded intro clip is missing from this build.";
        return false;
    }

    const std::string playlist = IntroPlaylistXml();
    return WriteBytes(VideoPath(game, INTRO_CLIP), clip, error) &&
           WriteBytes(VideoPath(game, INTRO_PLAYLIST), {reinterpret_cast<const uint8_t*>(playlist.data()), playlist.size()}, error);
}

bool UninstallIntroFiles(const Game& game, std::wstring& error) {
    return DeleteIfPresent(VideoPath(game, INTRO_CLIP), error) && DeleteIfPresent(VideoPath(game, INTRO_PLAYLIST), error);
}

}  // namespace

std::filesystem::path GamePakPath(const Game& game) {
    const wchar_t* folder = L"data";

    if (game.id == GameId::Base) {
        folder = L"datals";
    }

    if (game.id == GameId::HammersOfFate) {
        folder = L"dataa1";
    }

    return game.root / folder / L"" OWN_PAK_NAME;
}

bool ReadFontsOption(const Game& game) {
    return GetPrivateProfileIntW(L"options", L"fonts", 1, IniPath(game).c_str()) != 0;
}

bool InstallGamePak(const Game& game, bool fonts, bool skipIntro, bool keepSharedIntro, std::wstring& error) {
    const auto recipe = EmbeddedRecipe(game.id);
    if (recipe.empty()) {
        error = L"the embedded layout recipe is missing from this build.";
        return false;
    }

    const GamePakOptions options{fonts, skipIntro};

    if (!BuildGamePak(game.id, game.root, recipe, options, GamePakPath(game), error)) {
        return false;
    }

    if (skipIntro ? !InstallIntroFiles(game, error) : !keepSharedIntro && !UninstallIntroFiles(game, error)) {
        return false;
    }

    const std::wstring ini = IniPath(game).wstring();

    if (!WritePrivateProfileStringW(L"options", L"fonts", fonts ? L"1" : L"0", ini.c_str()) ||
        !WritePrivateProfileStringW(L"pak", L"game_paks", GamePaksFingerprint(game).c_str(), ini.c_str())) {
        error = ini + L": cannot write";
        return false;
    }

    return true;
}

bool GamePakComplete(const Game& game, bool skipIntro) {
    std::error_code ec{};

    if (!std::filesystem::exists(GamePakPath(game), ec)) {
        return false;
    }

    return !skipIntro || (std::filesystem::exists(VideoPath(game, INTRO_CLIP), ec) && std::filesystem::exists(VideoPath(game, INTRO_PLAYLIST), ec));
}

bool GamePakOutdated(const Game& game) {
    wchar_t stored[FINGERPRINT_SIZE]{};
    GetPrivateProfileStringW(L"pak", L"game_paks", L"", stored, FINGERPRINT_SIZE, IniPath(game).c_str());
    return GamePaksFingerprint(game) != stored;
}

bool UninstallGamePak(const Game& game, bool keepSharedIntro, std::wstring& error) {
    return DeleteIfPresent(GamePakPath(game), error) && (keepSharedIntro || UninstallIntroFiles(game, error));
}
