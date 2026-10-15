@echo off
set "MSVC=C:/Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "SDK=C:/Program Files (x86)\Windows Kits\10"
set "VER=10.0.26100.0"
set "PATH=%MSVC%\bin\Hostx64\x64;%SDK%\bin\%VER%\x64;%PATH%"
set "INCLUDE=%MSVC%\include;%SDK%\Include\%VER%\ucrt;%SDK%\Include\%VER%\um;%SDK%\Include\%VER%\shared;%SDK%\Include\%VER%\winrt;%SDK%\Include\%VER%\cppwinrt"
set "LIB=%MSVC%\lib\x64;%SDK%\Lib\%VER%\ucrt\x64;%SDK%\Lib\%VER%\um\x64"
del /s /q build\local-win\windows\driver\CMakeFiles\virtualcamera_driver.dir\*.obj 2>nul
set "CL=/DVCAM_DRIVER_TRACE"
cmake --build build\local-win --target virtualcamera_driver
set "CL="
