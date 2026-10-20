#include <windows.h>

namespace {

FARPROC RealExport(const char* name) {
    static HMODULE real = [] {
        wchar_t path[MAX_PATH];
        const UINT length = GetSystemDirectoryW(path, MAX_PATH);
        if (length == 0 || length + 13 >= MAX_PATH) return HMODULE{};
        lstrcatW(path, L"\\dinput8.dll");
        return LoadLibraryW(path);
    }();
    return real ? GetProcAddress(real, name) : nullptr;
}

template <typename Fn>
Fn Real(const char* name) {
    return reinterpret_cast<Fn>(RealExport(name));
}

}  // namespace

extern "C" {

HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE instance, DWORD version, const GUID& iid, void** out, void* outer) {
    const auto real = Real<decltype(&ProxyDirectInput8Create)>("DirectInput8Create");
    return real ? real(instance, version, iid, out, outer) : E_FAIL;
}

HRESULT WINAPI ProxyDllCanUnloadNow() {
    const auto real = Real<decltype(&ProxyDllCanUnloadNow)>("DllCanUnloadNow");
    return real ? real() : S_FALSE;
}

HRESULT WINAPI ProxyDllGetClassObject(const CLSID& clsid, const IID& iid, void** out) {
    const auto real = Real<decltype(&ProxyDllGetClassObject)>("DllGetClassObject");
    return real ? real(clsid, iid, out) : E_FAIL;
}

HRESULT WINAPI ProxyDllRegisterServer() {
    const auto real = Real<decltype(&ProxyDllRegisterServer)>("DllRegisterServer");
    return real ? real() : E_FAIL;
}

HRESULT WINAPI ProxyDllUnregisterServer() {
    const auto real = Real<decltype(&ProxyDllUnregisterServer)>("DllUnregisterServer");
    return real ? real() : E_FAIL;
}

const void* WINAPI ProxyGetdfDIJoystick() {
    const auto real = Real<decltype(&ProxyGetdfDIJoystick)>("GetdfDIJoystick");
    return real ? real() : nullptr;
}

}  // extern "C"
