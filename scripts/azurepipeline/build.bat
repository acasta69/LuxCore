:: Setting up dependencies
cd ..
git lfs install
git clone --branch master https://github.com/acasta69/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/acasta69/WindowsCompileDeps .\WindowsCompileDeps
dir C:\
dir "C:\Program Files"
dir "C:\Program Files (x86)"
mklink /J Luxcore .\s

copy .\Luxcore\cmake-build-x64.bat .\WindowsCompile\cmake-build-x64.cmd
copy .\Luxcore\create-standalone.bat .\WindowsCompile\create-standalone.cmd
cd WindowsCompile
call .\cmake-build-x64.cmd /no-ocl
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
