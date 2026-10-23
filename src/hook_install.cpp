#include "hook_install.h"

#include "product.h"

#include <windows.h>

#include <vector>

namespace {

constexpr const wchar_t* HOOK_DLL = L"dinput8.dll";
constexpr const wchar_t* INI = L"homm5-widescreen.ini";
constexpr const wchar_t* LOG = L"homm5-widescreen.log";

std::filesystem::path InGameDir(const Game& game, const wchar_t* name) {
    return game.exe.parent_path() / name;
}

std::wstring SystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring text = buffer ? buffer : L"error " + std::to_wstring(code);
    LocalFree(buffer);

    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) {
        text.pop_back();
    }

    if (code == ERROR_ACCESS_DENIED) {
        text += L" Run the patcher as administrator.";
    }

    if (code == ERROR_SHARING_VIOLATION) {
        text += L" Close the game first.";
    }

    return text;
}

bool Fail(std::wstring& error, const std::filesystem::path& path) {
    error = path.wstring() + L": " + SystemMessage(GetLastError());
    return false;
}

bool DeleteIfPresent(const std::filesystem::path& path, std::wstring& error) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
        return true;
    }

    return Fail(error, path);
}

const wchar_t* BuildTag(GameId id) {
    switch (id) {
        case GameId::Base: return L"base16";
        case GameId::HammersOfFate: return L"hof21";
        case GameId::TribesOfTheEast: break;
    }

    return L"toe31";
}

bool DeleteFallbackLog(const Game& game, std::wstring& error) {
    wchar_t localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return true;
    }

    const auto folder = std::filesystem::path(localAppData) / LOG_FALLBACK_FOLDER;

    if (!DeleteIfPresent(folder / FallbackLogName(BuildTag(game.id)), error)) {
        return false;
    }

    // Heroes 5.5 runs its own MMH55.exe from the ToE bin folder, so the same dinput8.dll logs under its tag.
    if (game.id == GameId::TribesOfTheEast && !DeleteIfPresent(folder / FallbackLogName(L"mmh55"), error)) {
        return false;
    }

    RemoveDirectoryW(folder.c_str());
    return true;
}

struct DllVersion {
    std::wstring productName;
    uint64_t fileVersion = 0;
};

DllVersion VersionOf(const std::filesystem::path& path) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return {};
    }

    std::vector<uint8_t> info(size);

    if (!GetFileVersionInfoW(path.c_str(), 0, size, info.data())) {
        return {};
    }

    DllVersion version{};
    wchar_t* name = nullptr;
    UINT length = 0;

    if (VerQueryValueW(info.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&name), &length)) {
        version.productName.assign(name, length ? length - 1 : 0);
    }

    VS_FIXEDFILEINFO* fixed = nullptr;

    if (VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &length) && length >= sizeof(*fixed)) {
        version.fileVersion = (uint64_t{fixed->dwFileVersionMS} << 32) | fixed->dwFileVersionLS;
    }

    return version;
}

constexpr uint64_t PackVersion(uint64_t major, uint64_t minor, uint64_t patch, uint64_t build) {
    return (major << 48) | (minor << 32) | (patch << 16) | build;
}

}  // namespace

std::filesystem::path HookDllPath(const Game& game) {
    return InGameDir(game, HOOK_DLL);
}

std::filesystem::path PackagedHookDll() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(std::wstring(path, length)).parent_path() / HOOK_DLL;
}

HookState QueryHook(const Game& game) {
    const auto dll = HookDllPath(game);
    std::error_code ec{};

    if (!std::filesystem::exists(dll, ec)) {
        return HookState::NotInstalled;
    }

    const DllVersion version = VersionOf(dll);
    if (version.productName != L"" PRODUCT_NAME) {
        return HookState::Foreign;
    }

    return version.fileVersion == PackVersion(PRODUCT_VERSION) ? HookState::Installed : HookState::Outdated;
}

bool ReadSkipFade(const Game& game) {
    return GetPrivateProfileIntW(L"options", L"skip_fade", 0, InGameDir(game, INI).c_str()) != 0;
}

bool WriteHookOptions(const Game& game, bool skipFade, std::wstring& error) {
    if (QueryHook(game) != HookState::Installed) {
        error = L"copy " + PackagedHookDll().wstring() + L" into " + game.exe.parent_path().wstring() + L" first.";
        return false;
    }

    const auto ini = InGameDir(game, INI);

    if (!WritePrivateProfileStringW(L"options", L"skip_fade", skipFade ? L"1" : L"0", ini.c_str())) {
        return Fail(error, ini);
    }

    return true;
}

bool UninstallHook(const Game& game, std::wstring& error) {
    const HookState state = QueryHook(game);
    if ((state == HookState::Installed || state == HookState::Outdated) && !DeleteIfPresent(HookDllPath(game), error)) {
        return false;
    }

    return DeleteIfPresent(InGameDir(game, INI), error) && DeleteIfPresent(InGameDir(game, LOG), error) &&
           DeleteFallbackLog(game, error);
}
