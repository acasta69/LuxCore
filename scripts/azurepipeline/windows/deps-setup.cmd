:: Setting up dependencies

cd ..
git lfs install
git clone --branch master https://github.com/acasta69/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/acasta69/WindowsCompileDeps .\WindowsCompileDeps

mklink /J Luxcore %SYSTEM_DEFAULTWORKINGDIRECTORY%

pip install --upgrade pip

pip install --upgrade setuptools

pip install wheel
pip install pyinstaller

:: pyluxcoretool will use same numpy version used to build LuxCore
pip install numpy==1.15.4

pip install PySide2
