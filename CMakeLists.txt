cmake_minimum_required(VERSION 3.10)

# Najpierw sprawdzamy środowisko
if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

# NAJWAŻNIEJSZE: Ładujemy narzędzia Vity ZANIM zdefiniujemy projekt
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# Dopiero teraz definiujemy projekt
project(myrient_browser)

add_executable(myrient_app main.c)

# flagi kompilatora
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

target_link_libraries(myrient_app
    curl ssl crypto z vita2d
    SceDisplay_stub SceGxm_stub SceSysmodule_stub
    SceCtrl_stub ScePgf_stub SceNet_stub SceCommonDialog_stub m
)

vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
