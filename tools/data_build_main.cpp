// Dev tool: homm5-data-build <base|hof> <game root> <recipe> <out pak>
#include "../src/data/data_build.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) {
        std::fwprintf(stderr, L"usage: homm5-data-build <base|hof> <game root> <recipe> <out pak>\n");
        return 2;
    }
    const std::wstring game = argv[1];
    if (game != L"base" && game != L"hof") {
        std::fwprintf(stderr, L"unknown game %ls\n", argv[1]);
        return 2;
    }
    std::ifstream in(argv[3], std::ios::binary);
    const std::vector<uint8_t> recipe((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::wstring error;
    if (!BuildDataPak(game == L"base" ? GameId::Base : GameId::HammersOfFate, argv[2], recipe, argv[4], error)) {
        std::fwprintf(stderr, L"failed: %ls\n", error.c_str());
        return 1;
    }
    std::fwprintf(stdout, L"wrote %ls\n", argv[4]);
    return 0;
}
