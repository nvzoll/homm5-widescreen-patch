#pragma once

#include <cstdint>
#include <span>

constexpr uintptr_t kPreferredBase = 0x400000;

struct ByteEdit {
    uintptr_t va;
    size_t length;
    uint8_t original[8];
    uint8_t patched[8];
    int absOffset;
};

class Image {
public:
    Image();

    uint32_t timestamp() const { return timestamp_; }
    uint32_t unwrappedSizeOfImage() const { return unwrappedSizeOfImage_; }
    bool steamStub() const { return steamStub_; }
    const char* store() const { return steamStub_ ? "Steam" : "GOG"; }
    bool IsBuild(uint32_t timestamp, uint32_t sizeOfImage) const {
        return timestamp_ == timestamp && unwrappedSizeOfImage_ == sizeOfImage;
    }

    void** ImportSlot(const char* dll, const char* function) const;

    template <typename T>
    T* At(uintptr_t va) const { return reinterpret_cast<T*>(va - kPreferredBase + base_); }

    // absOffsets: positions of 32-bit absolute addresses in `bytes`, rebased like the loader does.
    bool Matches(uintptr_t va, std::span<const uint8_t> bytes, std::span<const size_t> absOffsets = {}) const;
    void Write(uintptr_t va, std::span<const uint8_t> bytes, std::span<const size_t> absOffsets = {}) const;
    void WriteCall(uintptr_t va, const void* target, size_t length) const;
    void WriteJump(uintptr_t va, const void* target) const;
    void Nop(uintptr_t va, size_t length) const;

    bool MatchesAll(std::span<const ByteEdit> edits) const;
    void ApplyAll(std::span<const ByteEdit> edits) const;

private:
    void WriteBranch(uintptr_t va, uint8_t opcode, const void* target, size_t length) const;

    uintptr_t base_;
    uint32_t timestamp_;
    uint32_t unwrappedSizeOfImage_;
    bool steamStub_;
};
