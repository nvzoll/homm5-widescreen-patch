#include "hook_install.h"

#include "product.h"

#include <windows.h>

#include <vector>

namespace {

constexpr const wchar_t* kHookDll = L"dinput8.dll";
constexpr const wchar_t* kIni = L"homm5-widescreen.ini";
constexpr const wchar_t* kLog = L"homm5-widescreen.log";

std::filesystem::path InGameDir(const Game& game, const wchar_t* name) {
    return game.exe.parent_path() / name;
}

std::wstring SystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring text = buffer ? buffer : L"error " + std::to_wstring(code);
    LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) text.pop_back();
    if (code == ERROR_ACCESS_DENIED) text += L" Run the patcher as administrator.";
    if (code == ERROR_SHARING_VIOLATION) text += L" Close the game first.";
    return text;
}

bool Fail(std::wstring& error, const std::filesystem::path& path) {
    error = path.wstring() + L": " + SystemMessage(GetLastError());
    return false;
}

bool DeleteIfPresent(const std::filesystem::path& path, std::wstring& error) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) return true;
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
    wchar_t localAppData[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return true;
    const auto folder = std::filesystem::path(localAppData) / LOG_FALLBACK_FOLDER;
    if (!DeleteIfPresent(folder / FallbackLogName(BuildTag(game.id)), error)) return false;
    RemoveDirectoryW(folder.c_str());
    return true;
}

std::wstring ProductNameOf(const std::filesystem::path& path) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) return {};
    std::vector<uint8_t> info(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, info.data())) return {};
    wchar_t* value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(info.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&value), &length)) return {};
    return {value, length ? length - 1 : 0};
}

}  // namespace

std::filesystem::path HookDllPath(const Game& game) {
    return InGameDir(game, kHookDll);
}

std::filesystem::path PackagedHookDll() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(std::wstring(path, length)).parent_path() / kHookDll;
}

HookState QueryHook(const Game& game) {
    const auto dll = HookDllPath(game);
    if (!std::filesystem::exists(dll)) return HookState::NotInstalled;
    return ProductNameOf(dll) == L"" PRODUCT_NAME ? HookState::Installed : HookState::Foreign;
}

bool ReadSkipFade(const Game& game) {
    return GetPrivateProfileIntW(L"options", L"skip_fade", 0, InGameDir(game, kIni).c_str()) != 0;
}

bool WriteHookOptions(const Game& game, bool skipFade, std::wstring& error) {
    if (QueryHook(game) != HookState::Installed) {
        error = L"copy " + PackagedHookDll().wstring() + L" into " + game.exe.parent_path().wstring() + L" first.";
        return false;
    }
    const auto ini = InGameDir(game, kIni);
    if (!WritePrivateProfileStringW(L"options", L"skip_fade", skipFade ? L"1" : L"0", ini.c_str())) return Fail(error, ini);
    return true;
}

bool UninstallHook(const Game& game, std::wstring& error) {
    if (QueryHook(game) == HookState::Installed && !DeleteIfPresent(HookDllPath(game), error)) return false;
    return DeleteIfPresent(InGameDir(game, kIni), error) && DeleteIfPresent(InGameDir(game, kLog), error) &&
           DeleteFallbackLog(game, error);
}
