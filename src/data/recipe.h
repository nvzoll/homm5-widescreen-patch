#pragma once

#include "pak.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Rebuilds the entries of one of our data paks from the user's own game data (format: tools/gen_recipe.py).
bool ApplyRecipe(std::span<const uint8_t> recipe, const PakArchives& sources, std::vector<PakFile>& files,
                 uint32_t& dosDateTime, std::wstring& error);
