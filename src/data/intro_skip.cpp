#include "intro_skip.h"

#include <string_view>

namespace {

constexpr char kGameRoot[] = "UI/UIGameRoot.(UIGameRoot).xdb";
constexpr std::string_view kIntroId = "<ID>intro</ID>";
constexpr std::string_view kMovieOpen = "<MovieDesc>";
constexpr std::string_view kMovieClose = "</MovieDesc>";

}  // namespace

bool BuildIntroSkip(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error) {
    const auto data = game.Read(kGameRoot, error);
    if (!data) {
        if (error.empty()) {
            error = L"UI/UIGameRoot.(UIGameRoot).xdb: missing from the game data";
        }

        return false;
    }

    std::string xml(data->begin(), data->end());
    const size_t id = xml.find(kIntroId);
    const size_t open = id == std::string::npos ? id : xml.find(kMovieOpen, id);
    const size_t close = open == std::string::npos ? open : xml.find(kMovieClose, open);
    if (close == std::string::npos || xml.find("</Item>", id) < close) {
        error = L"UI/UIGameRoot.(UIGameRoot).xdb: no intro movie entry";
        return false;
    }

    const size_t value = open + kMovieOpen.size();
    xml.replace(value, close - value, std::string("../video/") + kIntroPlaylist);
    files.push_back({kGameRoot, {xml.begin(), xml.end()}});
    return true;
}

std::string IntroPlaylistXml() {
    std::string clip = kIntroClip;
    clip.resize(clip.size() - 4);
    return "<?xml version=\"1.0\"?>\r\n<Base>\r\n\t<Movies>\r\n\t\t<Item>\r\n\t\t\t<CanSkipMovie>1</CanSkipMovie>\r\n"
           "\t\t\t<CanSkipSequence>1</CanSkipSequence>\r\n\t\t\t<FileName>../video/" + clip + "</FileName>\r\n"
           "\t\t</Item>\r\n\t</Movies>\r\n</Base>\r\n";
}
