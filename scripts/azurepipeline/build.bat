:: Clone WindowsCompile
cd ..
git clone --branch master https://github.com/acasta69/WindowsCompile .\WindowsCompile
git -c filter.lfs.smudge= -c filter.lfs.required=false -c diff.mnemonicprefix=false -c core.quotepath=false --no-optional-locks clone --branch master https://github.com/acasta69/WindowsCompileDeps .\WindowsCompileDeps
git lfs pull .\WindowsCompileDeps

mklink /J Luxcore .\s

dir
dir Luxcore
dir /S WindowsCompileDeps\x64
:: Clone LuxCore (this is a bit a waste but WindowsCompile procedure
:: doesn't work with symbolic links)
::git clone https://github.com/acasta69/Luxcore.git
copy .\Luxcore\cmake-build-x64.bat .\WindowsCompile\cmake-build-x64.cmd
cd WindowsCompile
::cmd /k ""C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat""
call .\cmake-build-x64.cmd /no-ocl
exit
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
