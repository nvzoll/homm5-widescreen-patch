#include "pak.h"

#include <iowin32.h>
#include <zip.h>

namespace {

std::wstring Widen(const std::string& text) {
    return {text.begin(), text.end()};
}

std::string Lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    return text;
}

bool Fail(std::wstring& error, const std::filesystem::path& path, const std::wstring& what) {
    error = path.wstring() + L": " + what;
    return false;
}

}  // namespace

PakArchives::~PakArchives() {
    for (const unzFile pak : paks_) {
        unzClose(pak);
    }
}

bool PakArchives::Add(const std::filesystem::path& pak, int priority, std::wstring& error) {
    zlib_filefunc64_def io{};
    fill_win32_filefunc64W(&io);
    const unzFile zip = unzOpen2_64(pak.c_str(), &io);
    if (!zip) {
        return Fail(error, pak, L"cannot open as a zip archive");
    }

    const size_t index = paks_.size();
    paks_.push_back(zip);
    paths_.push_back(pak);

    char name[1024]{};

    for (int status = unzGoToFirstFile(zip); status == UNZ_OK; status = unzGoToNextFile(zip)) {
        unz_file_info64 info{};

        if (unzGetCurrentFileInfo64(zip, &info, name, sizeof name, nullptr, 0, nullptr, 0) != UNZ_OK) {
            return Fail(error, pak, L"corrupt central directory");
        }

        Location location{priority, static_cast<uint32_t>(info.dosDate), index, {}, info.uncompressed_size};
        unzGetFilePos64(zip, &location.position);

        const auto [it, inserted] = files_.try_emplace(Lower(name), location);
        const Location& current = it->second;

        const bool byDate = priority == current.priority || priority == TEXT_PRIORITY || current.priority == TEXT_PRIORITY;
        if (!inserted && (byDate ? location.dosDateTime > current.dosDateTime : priority > current.priority)) {
            it->second = location;
        }
    }

    return true;
}

std::optional<std::vector<uint8_t>> PakArchives::Read(const std::string& name, std::wstring& error) const {
    const auto it = files_.find(Lower(name));
    if (it == files_.end()) {
        return std::nullopt;
    }

    const Location& location = it->second;
    const unzFile zip = paks_[location.pak];
    unz64_file_pos position = location.position;
    std::vector<uint8_t> data(static_cast<size_t>(location.size));

    if (unzGoToFilePos64(zip, &position) != UNZ_OK || unzOpenCurrentFile(zip) != UNZ_OK) {
        Fail(error, paths_[location.pak], L"cannot open entry " + Widen(name));
        return std::nullopt;
    }

    const int read = data.empty() ? 0 : unzReadCurrentFile(zip, data.data(), static_cast<unsigned>(data.size()));
    const int closed = unzCloseCurrentFile(zip);
    if (read != static_cast<int>(data.size()) || closed != UNZ_OK) {
        Fail(error, paths_[location.pak], L"cannot read entry " + Widen(name) + L" (CRC or data error)");
        return std::nullopt;
    }

    return data;
}

bool WritePak(const std::filesystem::path& path, const std::vector<PakFile>& files, uint32_t dosDateTime, std::wstring& error) {
    zlib_filefunc64_def io{};
    fill_win32_filefunc64W(&io);

    const zipFile zip = zipOpen2_64(path.c_str(), APPEND_STATUS_CREATE, nullptr, &io);
    if (!zip) {
        return Fail(error, path, L"cannot create");
    }

    bool ok = true;

    for (const PakFile& file : files) {
        zip_fileinfo info{};
        info.dosDate = dosDateTime;

        ok = zipOpenNewFileInZip64(zip, file.name.c_str(), &info, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION, 0) == ZIP_OK &&
             zipWriteInFileInZip(zip, file.data.data(), static_cast<unsigned>(file.data.size())) == ZIP_OK &&
             zipCloseFileInZip(zip) == ZIP_OK;
        if (!ok) {
            Fail(error, path, L"cannot write entry " + Widen(file.name));
            break;
        }
    }

    if (zipClose(zip, nullptr) != ZIP_OK && ok) {
        ok = Fail(error, path, L"cannot finish the archive");
    }

    if (!ok) {
        std::error_code ec{};
        std::filesystem::remove(path, ec);
    }

    return ok;
}
