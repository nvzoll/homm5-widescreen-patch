#pragma once

#include "pak.h"

#include <string>
#include <vector>

inline constexpr char kIntroPlaylist[] = "homm5-intro.xml";
inline constexpr char kIntroClip[] = "homm5-blank.ogg";

// Points the game's "intro" movie entry at our own playlist in video\ (shipped playlists stay untouched).
bool BuildIntroSkip(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error);
std::string IntroPlaylistXml();
