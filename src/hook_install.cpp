#include "hook_install.h"

#include "product.h"

#include <windows.h>

#include <span>
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

std::span<const uint8_t> EmbeddedHookDll() {
    const HRSRC resource = FindResourceW(nullptr, L"HOOK_DLL", RT_RCDATA);
    if (!resource) return {};
    const HGLOBAL loaded = LoadResource(nullptr, resource);
    return {static_cast<const uint8_t*>(LockResource(loaded)), SizeofResource(nullptr, resource)};
}

bool WriteFileBytes(const std::filesystem::path& path, std::span<const uint8_t> bytes, std::wstring& error) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, path);
    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    const DWORD code = GetLastError();
    CloseHandle(file);
    if (!ok || written != bytes.size()) {
        SetLastError(code);
        return Fail(error, path);
    }
    return true;
}

bool DeleteIfPresent(const std::filesystem::path& path, std::wstring& error) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return true;
    return Fail(error, path);
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

HookState QueryHook(const Game& game) {
    const auto dll = InGameDir(game, kHookDll);
    if (!std::filesystem::exists(dll)) return HookState::NotInstalled;
    return ProductNameOf(dll) == L"" PRODUCT_NAME ? HookState::Installed : HookState::Foreign;
}

bool ReadSkipFade(const Game& game) {
    return GetPrivateProfileIntW(L"options", L"skip_fade", 0, InGameDir(game, kIni).c_str()) != 0;
}

bool InstallHook(const Game& game, bool skipFade, std::wstring& error) {
    if (QueryHook(game) == HookState::Foreign) {
        error = InGameDir(game, kHookDll).wstring() + L" belongs to another mod; remove it first.";
        return false;
    }
    const auto dll = EmbeddedHookDll();
    if (dll.empty()) {
        error = L"the embedded hook DLL is missing from this build.";
        return false;
    }
    if (!WriteFileBytes(InGameDir(game, kHookDll), dll, error)) return false;
    const auto ini = InGameDir(game, kIni);
    if (!WritePrivateProfileStringW(L"options", L"skip_fade", skipFade ? L"1" : L"0", ini.c_str())) return Fail(error, ini);
    return true;
}

bool UninstallHook(const Game& game, std::wstring& error) {
    if (QueryHook(game) == HookState::Installed && !DeleteIfPresent(InGameDir(game, kHookDll), error)) return false;
    return DeleteIfPresent(InGameDir(game, kIni), error) && DeleteIfPresent(InGameDir(game, kLog), error);
}
