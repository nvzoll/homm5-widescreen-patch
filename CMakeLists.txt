cmake_minimum_required(VERSION 3.24)
project(homm5_widescreen_patch LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/product.h)
file(STRINGS src/product.h PRODUCT_VERSION_LINE REGEX "^#define PRODUCT_VERSION ")
string(REGEX REPLACE "^#define PRODUCT_VERSION +([0-9]+), *([0-9]+), *([0-9]+), *([0-9]+).*$" "\\1.\\2.\\3.\\4" PRODUCT_MANIFEST_VERSION "${PRODUCT_VERSION_LINE}")
if(NOT PRODUCT_MANIFEST_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "cannot read PRODUCT_VERSION from src/product.h")
endif()
configure_file(src/app.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/app.manifest @ONLY)

add_library(zlib_minizip STATIC
    third_party/zlib/adler32.c
    third_party/zlib/compress.c
    third_party/zlib/crc32.c
    third_party/zlib/deflate.c
    third_party/zlib/inffast.c
    third_party/zlib/inflate.c
    third_party/zlib/inftrees.c
    third_party/zlib/trees.c
    third_party/zlib/uncompr.c
    third_party/zlib/zutil.c
    third_party/minizip/ioapi.c
    third_party/minizip/iowin32.c
    third_party/minizip/unzip.c
    third_party/minizip/zip.c
)
target_include_directories(zlib_minizip PUBLIC third_party/zlib third_party/minizip)
target_compile_definitions(zlib_minizip PUBLIC NOCRYPT NOUNCRYPT PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
target_compile_options(zlib_minizip PRIVATE /W0)

add_library(homm5-widescreen-hook SHARED
    src/hook/dllmain.cpp
    src/hook/proxy.cpp
    src/hook/image.cpp
    src/hook/base16.cpp
    src/hook/hof21.cpp
    src/hook/toe31.cpp
    src/hook/mmh55.cpp
    src/hook/own_pak.cpp
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
    src/pak_install.cpp
    src/app.rc
    ${CMAKE_CURRENT_BINARY_DIR}/app.manifest
)
target_compile_definitions(homm5-widescreen-patch PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(homm5-widescreen-patch PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-widescreen-patch PRIVATE homm5-data bcrypt comctl32 version)

add_dependencies(homm5-widescreen-patch homm5-widescreen-hook)

add_library(homm5-data STATIC
    src/data/pak.cpp
    src/data/recipe.cpp
    src/data/data_build.cpp
    src/data/game_paks.cpp
    src/data/font_build.cpp
    src/data/intro_skip.cpp
)
target_compile_definitions(homm5-data PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(homm5-data PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-data PUBLIC zlib_minizip gdi32)

add_executable(homm5-data-build tools/data_build_main.cpp)
target_compile_options(homm5-data-build PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-data-build PRIVATE homm5-data)

add_executable(homm5-font-build tools/font_build_main.cpp)
target_compile_options(homm5-font-build PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(homm5-font-build PRIVATE homm5-data)
