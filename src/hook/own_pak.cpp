#include "own_pak.h"

#include "../product.h"

#include <windows.h>

#include <cstring>

namespace {

// Above every hard-coded pak (base tops out at 4, HoF at 3). Text paks (-1) still compare by date; ours is newer.
constexpr int kOwnPakPriority = 100;

struct GameString {
    const char* begin;
    const char* end;
    const char* capacity;
};

using MountPattern = void(__thiscall*)(void* vfs, const GameString* pattern, int priority);

char g_path[MAX_PATH * 2];
GameString g_pattern;
MountPattern g_mountPattern;
uintptr_t g_factory;

void __stdcall MountOwnPak(void* vfs) {
    if (vfs) g_mountPattern(vfs, &g_pattern, kOwnPakPriority);
}

// Base factory: ecx, edx, one stack argument, ret 4. Only eax is used after the call.
__declspec(naked) void FactoryWithStackArgStub() {
    __asm {
        push dword ptr [esp + 4]
        call dword ptr [g_factory]
        push eax
        push eax
        call MountOwnPak
        pop eax
        ret 4
    }
}

// HoF factory: ecx, edx only.
__declspec(naked) void FactoryStub() {
    __asm {
        call dword ptr [g_factory]
        push eax
        push eax
        call MountOwnPak
        pop eax
        ret
    }
}

bool BuildPath(const char* folder) {
    char exe[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return false;
    std::string root(exe, length);
    for (int i = 0; i < 2; ++i) {
        const size_t slash = root.find_last_of('\\');
        if (slash == std::string::npos) return false;
        root.resize(slash);
    }
    const std::string path = root + "\\" + folder + "\\" OWN_PAK_NAME;
    if (path.size() >= sizeof g_path) return false;
    std::memcpy(g_path, path.c_str(), path.size() + 1);
    g_pattern = {g_path, g_path + path.size(), g_path + sizeof g_path};
    return true;
}

}  // namespace

void InstallOwnPak(const Image& image, const VfsFactorySite& site, std::string& log) {
    if (!image.Matches(site.call, site.original)) {
        log += "  own pak: unexpected bytes, skipped\r\n";
        return;
    }
    if (!BuildPath(site.folder)) {
        log += "  own pak: cannot build the pak path, skipped\r\n";
        return;
    }
    g_factory = reinterpret_cast<uintptr_t>(image.At<void>(site.factory));
    g_mountPattern = reinterpret_cast<MountPattern>(image.At<void>(site.mountPattern));
    const void* stub = site.factoryHasStackArg ? reinterpret_cast<const void*>(&FactoryWithStackArgStub) : reinterpret_cast<const void*>(&FactoryStub);
    image.WriteCall(site.call, stub, site.original.size());
    log += std::string("  own pak: ") + g_path + "\r\n";
}
