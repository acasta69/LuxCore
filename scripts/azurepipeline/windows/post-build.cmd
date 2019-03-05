:: Gathering and packing binaries
for /f "tokens=2 delims=_" %%a in ('git tag --points-at HEAD') do set GITHUB_TAG=%%a
echo %GITHUB_TAG%
cd ..\WindowsCompile
call create-standalone.bat

if "%1" EQU "/no-ocl" (
    set LUX_LATEST=luxcorerender-%GITHUB_TAG%-win64
) else (
    set LUX_LATEST=luxcorerender-%GITHUB_TAG%-win64-opencl
)

move %DIR% %LUX_LATEST%
.\support\bin\7za.exe a %LUX_LATEST%.zip %LUX_LATEST%
