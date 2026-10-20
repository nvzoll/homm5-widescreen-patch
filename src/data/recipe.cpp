#include "recipe.h"

#include <zlib.h>

#include <cstring>

namespace {

enum Kind : uint8_t { kNew = 0, kPatch = 1 };

class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) : data_(data) {}

    bool ok() const { return ok_; }

    template <typename T>
    T Get() {
        T value{};
        if (!Has(sizeof value)) return value;
        std::memcpy(&value, data_.data() + pos_, sizeof value);
        pos_ += sizeof value;
        return value;
    }

    std::span<const uint8_t> Bytes(size_t length) {
        if (!Has(length)) return {};
        const auto bytes = data_.subspan(pos_, length);
        pos_ += length;
        return bytes;
    }

private:
    bool Has(size_t length) {
        ok_ = ok_ && length <= data_.size() - pos_;
        return ok_;
    }

    std::span<const uint8_t> data_;
    size_t pos_ = 0;
    bool ok_ = true;
};

std::wstring Widen(const std::string& text) {
    return {text.begin(), text.end()};
}

bool Patch(Reader& recipe, const std::string& name, const PakArchives& sources, std::vector<uint8_t>& out, std::wstring& error) {
    const auto size = recipe.Get<uint32_t>();
    const auto crc = recipe.Get<uint32_t>();
    const auto hunks = recipe.Get<uint32_t>();
    const auto source = sources.Read(name, error);
    if (!source) {
        if (error.empty()) error = Widen(name) + L": missing from the game data";
        return false;
    }
    if (source->size() != size || crc32(0, source->data(), static_cast<uInt>(source->size())) != crc) {
        error = Widen(name) + L": the game's copy differs from the one this patch was made for";
        return false;
    }
    size_t pos = 0;
    for (uint32_t i = 0; i < hunks && recipe.ok(); ++i) {
        const auto offset = recipe.Get<uint32_t>();
        const auto remove = recipe.Get<uint32_t>();
        const auto insert = recipe.Bytes(recipe.Get<uint32_t>());
        if (offset < pos || offset + uint64_t{remove} > source->size()) {
            error = Widen(name) + L": corrupt recipe hunk";
            return false;
        }
        out.insert(out.end(), source->begin() + pos, source->begin() + offset);
        out.insert(out.end(), insert.begin(), insert.end());
        pos = offset + remove;
    }
    out.insert(out.end(), source->begin() + pos, source->end());
    return true;
}

}  // namespace

bool ApplyRecipe(std::span<const uint8_t> data, const PakArchives& sources, std::vector<PakFile>& files,
                 uint32_t& dosDateTime, std::wstring& error) {
    Reader recipe(data);
    const auto magic = recipe.Bytes(8);
    const auto version = recipe.Get<uint32_t>();
    dosDateTime = recipe.Get<uint32_t>();
    const auto count = recipe.Get<uint32_t>();
    if (!recipe.ok() || std::memcmp(magic.data(), "H5RECIPE", 8) != 0 || version != 1) {
        error = L"unsupported recipe";
        return false;
    }
    files.clear();
    files.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const auto name = recipe.Bytes(recipe.Get<uint16_t>());
        PakFile file{std::string(name.begin(), name.end()), {}};
        const auto kind = recipe.Get<uint8_t>();
        if (kind == kNew) {
            const auto content = recipe.Bytes(recipe.Get<uint32_t>());
            file.data.assign(content.begin(), content.end());
        } else if (kind != kPatch || !Patch(recipe, file.name, sources, file.data, error)) {
            if (error.empty()) error = L"corrupt recipe";
            return false;
        }
        if (!recipe.ok()) {
            error = L"truncated recipe";
            return false;
        }
        files.push_back(std::move(file));
    }
    return true;
}
