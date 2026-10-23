#pragma once

#include <unzip.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// The game's merged view of several .pak (zip) archives; entry names are case-insensitive.
class PakArchives {
public:
    static constexpr int TEXT_PRIORITY = -1;

    PakArchives() = default;
    PakArchives(const PakArchives&) = delete;
    PakArchives& operator=(const PakArchives&) = delete;
    ~PakArchives();

    // Engine rule (HoF FUN_009eded0): if the priorities are equal or either is TEXT_PRIORITY, the newer
    // DOS timestamp wins, otherwise the higher priority; ties keep the earlier pak, so mount order matters.
    bool Add(const std::filesystem::path& pak, int priority, std::wstring& error);
    std::optional<std::vector<uint8_t>> Read(const std::string& name, std::wstring& error) const;

private:
    struct Location {
        int priority;
        uint32_t dosDateTime;
        size_t pak;
        unz64_file_pos position;
        uint64_t size;
    };

    std::vector<unzFile> paks_;
    std::vector<std::filesystem::path> paths_;
    std::unordered_map<std::string, Location> files_;
};

struct PakFile {
    std::string name;
    std::vector<uint8_t> data;
};

bool WritePak(const std::filesystem::path& path, const std::vector<PakFile>& files, uint32_t dosDateTime, std::wstring& error);
