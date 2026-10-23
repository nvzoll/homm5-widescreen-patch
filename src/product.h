#pragma once

#define PRODUCT_NAME "HoMM5 Widescreen Patch"
#define PRODUCT_VERSION_TEXT "0.3.0"
#define PRODUCT_VERSION 0, 3, 0, 0
#define OWN_PAK_NAME "homm5-widescreen.pak"
#define LOG_FALLBACK_FOLDER "homm5-widescreen"

#ifndef RC_INVOKED
#include <string>

inline std::wstring FallbackLogName(const wchar_t* build) {
    return std::wstring(L"homm5-widescreen-") + build + L".log";
}
#endif
