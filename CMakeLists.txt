cmake_minimum_required(VERSION 3.24)
project(homm5_widescreen_patch LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(homm5-widescreen-patch WIN32
    src/main.cpp
    src/games.cpp
    src/sha256.cpp
    src/app.rc
    src/app.manifest
)
target_compile_definitions(homm5-widescreen-patch PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(homm5-widescreen-patch PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-widescreen-patch PRIVATE bcrypt comctl32)
