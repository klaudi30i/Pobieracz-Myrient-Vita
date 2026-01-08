cmake_minimum_required(VERSION 3.10)

# Standardowy blok z oficjalnych przykładów VitaSDK
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path")
  endif()
endif()

include("${CMAKE_TOOLCHAIN_FILE}")

project(myrient_browser C)

add_executable(myrient_app main.c)

# Flagi kompilatora (standardowe dla Vity)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

# Biblioteki w odpowiedniej kolejności (curl wymaga ssl/crypto/z)
target_link_libraries(myrient_app
    curl
    ssl
    crypto
    z
    vita2d
    png
    jpeg
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
