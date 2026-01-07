cmake_minimum_required(VERSION 3.10)

# 1. Sprawdzenie środowiska
if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

# 2. Ręczne ustawienie kompilatorów (to naprawiło poprzedni błąd)
set(CMAKE_C_COMPILER "$ENV{VITASDK}/bin/arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "$ENV{VITASDK}/bin/arm-vita-eabi-g++")

# 3. Ładowanie skryptu Vity
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# 4. NOWOŚĆ: Ręczne dodanie ścieżek do plików .h (Headerów)
# To naprawia błąd "fatal error: psp2/kernel/process.h: No such file or directory"
include_directories(
    "$ENV{VITASDK}/arm-vita-eabi/include"
    "$ENV{VITASDK}/include"
    "/usr/local/vitasdk/arm-vita-eabi/include"
)

# 5. Definicja projektu
project(myrient_browser C CXX)

# 6. Flagi
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

# 7. Plik wykonywalny
add_executable(myrient_app main.c)

# 8. Biblioteki
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

# 9. Budowanie VPK
vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
