cmake_minimum_required(VERSION 3.10)

# 1. Sprawdzamy zmienną środowiskową (to musi być na górze)
if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

# 2. Ładujemy skrypt Vity
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# 3. FIX: Ręcznie wymuszamy ścieżki do kompilatorów
# To naprawia błąd "No CMAKE_C_COMPILER could be found"
set(CMAKE_C_COMPILER "arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-vita-eabi-g++")

# 4. Definiujemy projekt (Dopiero teraz!)
project(myrient_browser C CXX)

# 5. Flagi kompilacji (optymalizacja i flagi dla Vity)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3")

# 6. Tworzymy plik wykonywalny z main.c
add_executable(myrient_app main.c)

# 7. Linkujemy biblioteki (kolejność jest ważna!)
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

# 8. Tworzymy plik VPK
vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
