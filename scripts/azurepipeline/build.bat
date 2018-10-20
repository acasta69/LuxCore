:: Setting up dependencies
cd ..
git lfs install
git clone --branch master https://github.com/acasta69/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/acasta69/WindowsCompileDeps .\WindowsCompileDeps

mklink /J Luxcore .\s

"C:\Program Files\7-Zip\7z.exe"
"C:\Program Files (x86)\7-Zip\7z.exe"

copy .\Luxcore\cmake-build-x64.bat .\WindowsCompile\cmake-build-x64.cmd
cd WindowsCompile
call .\cmake-build-x64.cmd /no-ocl
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
