#include "game_paks.h"

#include "../product.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <span>
#include <vector>

namespace {

struct TablePak {
    const wchar_t* name;
    int priority;
};

constexpr int TEXTS = PakArchives::TEXT_PRIORITY;

// Hard-coded tables, mounted after data\*.pak (priority 0): base FUN_006c4a60 (datals\), HoF FUN_006ec930 (dataa1\).
// ToE only scans data\*.pak.
constexpr std::array BASE_TABLE = {
    TablePak{L"p5-texts.pak", TEXTS}, TablePak{L"p5-data.pak", 1}, TablePak{L"p6-texts.pak", TEXTS},
    TablePak{L"p6-data.pak", 2},      TablePak{L"p7-texts.pak", TEXTS}, TablePak{L"p7-data.pak", 3},
    TablePak{L"p8-texts.pak", TEXTS}, TablePak{L"p8-data.pak", 4},
};
constexpr std::array HOF_TABLE = {
    TablePak{L"texts.pak", TEXTS},    TablePak{L"p0-data.pak", 1},     TablePak{L"p1-data.pak", 1},
    TablePak{L"p2-data.pak", 1},      TablePak{L"p3-data.pak", 1},     TablePak{L"p0-texts.pak", TEXTS},
    TablePak{L"p1-texts.pak", TEXTS}, TablePak{L"p2-texts.pak", TEXTS}, TablePak{L"p3-texts.pak", TEXTS},
    TablePak{L"a1-texts.pak", 2},     TablePak{L"a1-sound.pak", 2},    TablePak{L"a1-data.pak", 2},
    TablePak{L"a1p1-data.pak", 3},    TablePak{L"a1p1-texts.pak", TEXTS}, TablePak{L"a1p1-sound.pak", 3},
};

std::wstring Lower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return text;
}

std::wstring Upper(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return text;
}

bool IsOwn(const std::filesystem::path& pak) {
    return Lower(pak.filename().wstring()) == L"" OWN_PAK_NAME;
}

}  // namespace

std::vector<GamePak> GamePakFiles(GameId game, const std::filesystem::path& root) {
    std::vector<std::filesystem::path> scanned{};
    std::error_code ec{};

    for (std::filesystem::directory_iterator it(root / L"data", ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code typeError{};

        if (it->is_regular_file(typeError) && Lower(it->path().extension().wstring()) == L".pak" && !IsOwn(it->path())) {
            scanned.push_back(it->path());
        }
    }

    if (ec || scanned.empty()) {
        return {};
    }

    // FindFirstFile order on NTFS (upper-cased names); the game scans the folder the same way.
    std::sort(scanned.begin(), scanned.end(), [](const auto& a, const auto& b) { return Upper(a.filename().wstring()) < Upper(b.filename().wstring()); });

    std::vector<GamePak> paks{};

    for (const auto& pak : scanned) {
        paks.push_back({pak, 0});
    }

    std::span<const TablePak> table{};
    std::filesystem::path folder{};

    switch (game) {
        case GameId::Base: table = BASE_TABLE, folder = root / L"datals"; break;
        case GameId::HammersOfFate: table = HOF_TABLE, folder = root / L"dataa1"; break;
        case GameId::TribesOfTheEast: break;
    }

    for (const TablePak& entry : table) {
        const auto pak = folder / entry.name;

        if (!IsOwn(pak) && std::filesystem::is_regular_file(pak, ec)) {
            paks.push_back({pak, entry.priority});
        }
    }

    return paks;
}

bool MountGamePaks(GameId game, const std::filesystem::path& root, PakArchives& archives, std::wstring& error) {
    const std::vector<GamePak> paks = GamePakFiles(game, root);
    if (paks.empty()) {
        error = (root / L"data").wstring() + L": no game paks found";
        return false;
    }

    for (const GamePak& pak : paks) {
        if (!archives.Add(pak.path, pak.priority, error)) {
            return false;
        }
    }

    return true;
}
