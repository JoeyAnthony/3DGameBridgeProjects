cmake -S . -B build ^
    -DsrDirectX_DIR="C:/Program Files/Simulated Reality/SDK/lib/cmake/srDirectX" ^
    -Dsimulatedreality_DIR="C:/Program Files/Simulated Reality/SDK/lib/cmake/simulatedreality" ^
    -DRESHADE_INCLUDE="C:/Users/Bram Teurlings/Documents/GitHub/reshade/include" ^
    -DIMGUI_INCLUDE="C:/Users/Bram Teurlings/Documents/GitHub/imgui" ^
    -DDX12_INCLUDE="C:/Users/Bram Teurlings/Documents/GitHub/DirectX-Headers/include" ^
	-G "Visual Studio 17 2022"
pause