cmake_minimum_required(VERSION 3.24)
project(homm5_widescreen_patch LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(homm5-widescreen-hook SHARED
    src/hook/dllmain.cpp
    src/hook/proxy.cpp
    src/hook/image.cpp
    src/hook/base16.cpp
    src/hook/hof21.cpp
    src/hook/toe31.cpp
    src/hook/exports.def
    src/hook/hook.rc
)
set_target_properties(homm5-widescreen-hook PROPERTIES OUTPUT_NAME dinput8)
target_compile_definitions(homm5-widescreen-hook PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(homm5-widescreen-hook PRIVATE /W4 /permissive- /utf-8)

add_executable(homm5-widescreen-patch WIN32
    src/main.cpp
    src/games.cpp
    src/sha256.cpp
    src/hook_install.cpp
    src/app.rc
    src/app.manifest
)
target_compile_definitions(homm5-widescreen-patch PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(homm5-widescreen-patch PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-widescreen-patch PRIVATE bcrypt comctl32 version)

add_dependencies(homm5-widescreen-patch homm5-widescreen-hook)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hook_dll_$<CONFIG>.rc
     CONTENT "HOOK_DLL RCDATA \"$<TARGET_FILE:homm5-widescreen-hook>\"\n")
target_sources(homm5-widescreen-patch PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/hook_dll_$<CONFIG>.rc)
