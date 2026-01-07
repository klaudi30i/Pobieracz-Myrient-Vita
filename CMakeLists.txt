cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

# Najpierw konfiguracja Vity
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# Potem nazwa projektu
project(myrient_browser C)

# Potem reszta
add_executable(myrient_app main.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

target_link_libraries(myrient_app
    curl
    ssl
    crypto
    z
    vita2d
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    SceCtrl_stub
    ScePgf_stub
    SceNet_stub
    SceCommonDialog_stub
    m
)

vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
