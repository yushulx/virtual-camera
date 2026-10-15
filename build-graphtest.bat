@echo off
set "MSVC=C:/Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "SDK=C:/Program Files (x86)\Windows Kits\10"
set "VER=10.0.26100.0"
set "PATH=%MSVC%\bin\Hostx64\x64;%SDK%\bin\%VER%\x64;%PATH%"
set "INCLUDE=%MSVC%\include;%SDK%\Include\%VER%\ucrt;%SDK%\Include\%VER%\um;%SDK%\Include\%VER%\shared;%SDK%\Include\%VER%\winrt;%SDK%\Include\%VER%\cppwinrt"
set "LIB=%MSVC%\lib\x64;%SDK%\Lib\%VER%\ucrt\x64;%SDK%\Lib\%VER%\um\x64"
cl /nologo /EHsc /O2 /DUNICODE /D_UNICODE tools\graph-test.cpp /Fe:build\graph-test.exe ole32.lib oleaut32.lib strmiids.lib
