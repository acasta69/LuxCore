:: Clone WindowsCompile
cd ..
git clone https://github.com/acasta69/WindowsCompile.git
git clone https://github.com/acasta69/WindowsCompileDeps.git

mklink /J Luxcore .\s

:: Clone LuxCore (this is a bit a waste but WindowsCompile procedure
:: doesn't work with symbolic links)
::git clone https://github.com/acasta69/Luxcore.git
copy .\Luxcore\cmake-build-x64.bat .\WindowsCompile\cmake-build-x64.cmd
cd WindowsCompile
set
::cmd /k ""C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat""
call .\cmake-build-x64.cmd
exit
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
