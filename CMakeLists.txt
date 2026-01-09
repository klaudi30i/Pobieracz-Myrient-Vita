cmake_minimum_required(VERSION 3.10)

# Nazwa projektu
project(MyrientDownloader C)

# Tytuł aplikacji (musi mieć 9 znaków)
set(VITA_TITLEID "MYRI00001")
set(VITA_APP_NAME "Myrient Downloader")

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Please define VITASDK environment variable")
endif()

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# Flagi kompilatora (optymalizacja i ostrzeżenia)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3 -ffast-math")

# Pliki źródłowe
set(SOURCE_FILES src/main.c)

# Tworzenie pliku wykonywalnego
add_executable(MyrientDownloader ${SOURCE_FILES})

# Linkowanie bibliotek (kolejność jest ważna!)
target_link_libraries(MyrientDownloader
    vita2d
    freetype
    png
    jpeg
    z
    m
    curl
    ssl
    crypto
    SceLibKernel_stub
    SceKernel_stub
    ScePower_stub
    SceAppMgr_stub
    SceCtrl_stub
    SceTouch_stub
    SceSysmodule_stub
    SceNet_stub
    SceNetCtl_stub
    SceCommonDialog_stub
    SceIo_stub
)

# Tworzenie paczki VPK
vita_create_vpk(MyrientDownloader
    TITLE ${VITA_APP_NAME}
    TITLEID ${VITA_TITLEID}
    # Opcjonalnie: odkomentuj poniższe, jeśli dodasz ikony do folderu assets
    # ICON assets/icon0.png
    # BACKGROUND assets/bg.png
)