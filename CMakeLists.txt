cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

project(myrient_browser C)

add_executable(myrient_app main.c)

# TWARDA LISTA BIBLIOTEK W IDEALNEJ KOLEJNOŚCI
# Zmieniłem kolejność: najpierw curl, potem ssl/crypto/zlib
target_link_libraries(myrient_app
    curl
    ssl
    crypto
    z
    vita2d
    png
    jpeg
    freetype
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    SceCtrl_stub
    ScePgf_stub
    SceNet_stub
    SceCommonDialog_stub
    m
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
