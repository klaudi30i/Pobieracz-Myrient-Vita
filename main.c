name: Build Vita App

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    container: vitasdk/vitasdk:latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Compile
      run: |
        # Tutaj jest zmiana - podajemy Toolchain File wprost
        cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake .
        make
        
    - name: Upload VPK
      uses: actions/upload-artifact@v4
      with:
        name: MyrientBrowser
        path: myrient_app.vpk
