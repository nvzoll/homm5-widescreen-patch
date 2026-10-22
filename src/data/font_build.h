#pragma once

#include "pak.h"

#include <string>
#include <vector>

// Palatino Linotype (from Windows) rendered over the game's own font templates, so localized
// character sets come along. Each call appends finished pak entries to `files`.
bool PalatinoInstalled();
bool BuildBodyFonts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error);
bool BuildBriefFonts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error);
bool BuildRestyledTexts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error);
