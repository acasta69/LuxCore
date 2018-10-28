set USERNAME
set OCL_ROOT
cmake --version
cmake --version | findstr /C:"cmake version"
for /F "usebackq tokens=3" %%G in (`cmd /c "%CMAKE%" --version ^| findstr /C:"cmake version"`) do set CMAKE_VER=%%G
for /F "tokens=1 delims=." %%G in ("%CMAKE_VER%") do set CMAKE_VN_MAJOR=%%G
echo We are using CMake version: %CMAKE_VN_MAJOR%

