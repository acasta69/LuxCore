set USERNAME
set OCL_ROOT
cmake --version
cmake --version | findstr /C:"cmake version"

echo Finding if CMake is installed...
for /f "tokens=*" %%a in ('where cmake') do SET CMAKE=%%~fa  

if exist "%CMAKE%" (
  echo CMake found at "%CMAKE%"
) else (
  for %%a in (..\WindowsCompileDeps\bin\CMake\bin\cmake.exe) do set CMAKE=%%~fa
)

if not exist "%CMAKE%" echo CMakeNotFound

for /F "tokens=3" %%G in ('cmd /c "%CMAKE%" --version ^| findstr /C:"cmake version"') do set CMAKE_VER=%%G
for /F "tokens=1 delims=." %%G in ("%CMAKE_VER%") do set CMAKE_VN_MAJOR=%%G
echo We are using CMake version: %CMAKE_VN_MAJOR%

