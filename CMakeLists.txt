cmake_minimum_required(VERSION 3.0)
project(myrient_browser)

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

add_executable(myrient_app main.c)

target_link_libraries(myrient_app
    curl ssl crypto z vita2d
    SceDisplay_stub SceGxm_stub SceSysmodule_stub
    SceCtrl_stub ScePgf_stub SceNet_stub SceCommonDialog_stub m
)

vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
