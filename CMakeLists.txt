cmake_minimum_required(VERSION 3.10)

# 1. Definiujemy projekt
project(myrient_browser C)

# 2. Ustawiamy sztywne ścieżki dla kontenera GitHub Actions
set(VITASDK "/usr/local/vitasdk")

# 3. Wymuszamy kompilator
set(CMAKE_C_COMPILER "${VITASDK}/bin/arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "${VITASDK}/bin/arm-vita-eabi-g++")

# 4. Ładujemy konfigurację Vity
include("${VITASDK}/share/vita.cmake" REQUIRED)

# 5. NAJWAŻNIEJSZE: Ręcznie wbijamy ścieżki do nagłówków w flagi kompilatora
# To naprawia błąd "No such file or directory" ignorując standardowe mechanizmy CMake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3 -I${VITASDK}/arm-vita-eabi/include -I${VITASDK}/include -D__VITA__")

# 6. Dodajemy plik wykonywalny
add_executable(myrient_app main.c)

# 7. Wymuszamy ścieżki do bibliotek
link_directories(
    "${VITASDK}/arm-vita-eabi/lib"
    "${VITASDK}/lib"
)

# 8. Linkujemy biblioteki
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

# 9. Tworzymy VPK
vita_create_vpk(myrient_app "MYRIENT00" "Myrient Browser" "eboot.bin" "param.sfo")
