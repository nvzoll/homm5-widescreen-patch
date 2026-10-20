#include "../product.h"
#include "hooks.h"
#include "image.h"

#include <windows.h>

#include <string>

namespace {

std::wstring SiblingPath(HMODULE self, const wchar_t* name) {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring result(path, length);
    result.resize(result.find_last_of(L'\\') + 1);
    return result + name;
}

Options ReadOptions(const std::wstring& ini) {
    Options options;
    options.skipFade = GetPrivateProfileIntW(L"options", L"skip_fade", 0, ini.c_str()) != 0;
    return options;
}

bool WriteLog(const std::wstring& path, const std::string& text) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
    return true;
}

void WriteFallbackLog(const wchar_t* build, const std::string& text) {
    wchar_t localAppData[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    const std::wstring folder = std::wstring(localAppData) + L"\\" LOG_FALLBACK_FOLDER;
    CreateDirectoryW(folder.c_str(), nullptr);
    WriteLog(folder + L"\\" + FallbackLogName(build), text);
}

void Install(HMODULE self) {
    const Image image;
    const Options options = ReadOptions(SiblingPath(self, L"homm5-widescreen.ini"));
    std::string log;
    const wchar_t* build = L"unknown";
    if (InstallBase16(image, options, log)) {
        build = L"base16";
    } else if (InstallHof21(image, options, log)) {
        build = L"hof21";
    } else if (InstallToe31(image, options, log)) {
        build = L"toe31";
    } else {
        char line[96];
        wsprintfA(line, "unknown build (timestamp %08x, image size %08x), nothing patched\r\n", image.timestamp(), image.sizeOfImage());
        log += line;
    }
    if (!WriteLog(SiblingPath(self, L"homm5-widescreen.log"), log)) WriteFallbackLog(build, log);
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        Install(instance);
    }
    return TRUE;
}
