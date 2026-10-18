#pragma once

#include <filesystem>
#include <optional>
#include <string>

std::optional<std::string> Sha256File(const std::filesystem::path& path);
