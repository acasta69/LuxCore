:: Clone WindowsCompile
git clone https://github.com/acasta69/WindowsCompile.git
git clone https://github.com/acasta69/WindowsCompileDeps.git

:: Clone LuxCore (this is a bit a waste but WindowsCompile procedure
:: doesn't work with symbolic links)
git clone https://github.com/acasta69/Luxcore.git
dir
cd WindowsCompile
./cmake-build-x64.bat
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
