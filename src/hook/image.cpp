#include "image.h"

#include <windows.h>

#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> Rebased(std::span<const uint8_t> bytes, std::span<const size_t> absOffsets, uint32_t delta) {
    std::vector<uint8_t> result(bytes.begin(), bytes.end());
    for (const size_t offset : absOffsets) {
        uint32_t value;
        std::memcpy(&value, result.data() + offset, sizeof value);
        value += delta;
        std::memcpy(result.data() + offset, &value, sizeof value);
    }
    return result;
}

std::span<const size_t> AbsOffsets(const ByteEdit& edit, const size_t& storage) {
    return {&storage, edit.absOffset >= 0 ? 1u : 0u};
}

}  // namespace

Image::Image() : base_(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base_ + dos->e_lfanew);
    timestamp_ = nt->FileHeader.TimeDateStamp;
    sizeOfImage_ = nt->OptionalHeader.SizeOfImage;
}

bool Image::Matches(uintptr_t va, std::span<const uint8_t> bytes, std::span<const size_t> absOffsets) const {
    const auto expected = Rebased(bytes, absOffsets, static_cast<uint32_t>(base_ - kPreferredBase));
    return std::memcmp(At<const uint8_t>(va), expected.data(), expected.size()) == 0;
}

void Image::Write(uintptr_t va, std::span<const uint8_t> bytes, std::span<const size_t> absOffsets) const {
    const auto rebased = Rebased(bytes, absOffsets, static_cast<uint32_t>(base_ - kPreferredBase));
    void* address = At<void>(va);
    DWORD oldProtect;
    VirtualProtect(address, rebased.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
    std::memcpy(address, rebased.data(), rebased.size());
    VirtualProtect(address, rebased.size(), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, rebased.size());
}

void Image::WriteCall(uintptr_t va, const void* target, size_t length) const {
    WriteBranch(va, 0xE8, target, length);
}

void Image::WriteJump(uintptr_t va, const void* target) const {
    WriteBranch(va, 0xE9, target, 5);
}

void Image::WriteBranch(uintptr_t va, uint8_t opcode, const void* target, size_t length) const {
    std::vector<uint8_t> bytes(length, 0x90);
    bytes[0] = opcode;
    const auto next = reinterpret_cast<uintptr_t>(At<uint8_t>(va)) + 5;
    const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - next);
    std::memcpy(bytes.data() + 1, &rel, sizeof rel);
    Write(va, bytes);
}

void Image::Nop(uintptr_t va, size_t length) const {
    Write(va, std::vector<uint8_t>(length, 0x90));
}

bool Image::MatchesAll(std::span<const ByteEdit> edits) const {
    for (const ByteEdit& edit : edits) {
        const size_t absOffset = static_cast<size_t>(edit.absOffset);
        if (!Matches(edit.va, {edit.original, edit.length}, AbsOffsets(edit, absOffset))) return false;
    }
    return true;
}

void Image::ApplyAll(std::span<const ByteEdit> edits) const {
    for (const ByteEdit& edit : edits) {
        const size_t absOffset = static_cast<size_t>(edit.absOffset);
        Write(edit.va, {edit.patched, edit.length}, AbsOffsets(edit, absOffset));
    }
}
