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

void WriteLog(const std::wstring& path, const std::string& text) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(file);
}

void Install(HMODULE self) {
    const Image image;
    const Options options = ReadOptions(SiblingPath(self, L"homm5-widescreen.ini"));
    std::string log;
    if (!InstallBase16(image, options, log) && !InstallHof21(image, options, log) && !InstallToe31(image, options, log)) {
        char line[96];
        wsprintfA(line, "unknown build (timestamp %08x, image size %08x), nothing patched\r\n", image.timestamp(), image.sizeOfImage());
        log += line;
    }
    WriteLog(SiblingPath(self, L"homm5-widescreen.log"), log);
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        Install(instance);
    }
    return TRUE;
}
