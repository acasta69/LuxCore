:: Gathering and packing binaries
cd ..\WindowsCompile
call create-standalone.cmd
.\support\bin\7za.exe a luxcorerender-latest-win64.zip %DIR%

