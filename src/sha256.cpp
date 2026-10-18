#include "sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <vector>

namespace {

struct AlgorithmHandle {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~AlgorithmHandle() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};

struct HashHandle {
    BCRYPT_HASH_HANDLE handle = nullptr;
    ~HashHandle() { if (handle) BCryptDestroyHash(handle); }
};

}  // namespace

std::optional<std::string> Sha256File(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    AlgorithmHandle algorithm;
    HashHandle hash;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) ||
        !BCRYPT_SUCCESS(BCryptCreateHash(algorithm.handle, &hash.handle, nullptr, 0, nullptr, 0, 0))) {
        return std::nullopt;
    }

    std::vector<char> buffer(1 << 20);
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<ULONG>(file.gcount());
        if (count && !BCRYPT_SUCCESS(BCryptHashData(hash.handle, reinterpret_cast<PUCHAR>(buffer.data()), count, 0))) {
            return std::nullopt;
        }
    }
    if (file.bad()) return std::nullopt;

    std::array<UCHAR, 32> digest{};
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash.handle, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
        return std::nullopt;
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string text;
    for (const UCHAR byte : digest) {
        text += kHex[byte >> 4];
        text += kHex[byte & 0xF];
    }
    return text;
}
