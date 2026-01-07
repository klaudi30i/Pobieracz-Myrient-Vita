cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

# Konfiguracja Vity
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

project(myrient_browser C)

# Twoje biblioteki, które zainstalowaliśmy w build.yml
set(LIBRARIES
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

# Tworzenie pliku
add_executable(myrient_app main.c)
target_link_libraries(myrient_app ${LIBRARIES})

# Pakowanie do VPK
vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
