@echo off
set SCRIPT_DIR=%~dp0
set VC_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build
set QT_BIN=C:\Qt\6.5.1\msvc2019_64\bin
set QT_TOOLCHAIN=%QT_BIN%\..\lib\cmake\Qt6\qt.toolchain.cmake
set PATH=%QT_BIN%;%PATH%

call "%VC_BIN%\vcvarsall.bat" x64

:: cleanup build dir and prep for new build
rmdir build-win64 /s /q  2> nul
mkdir build-win64
cd build-win64

:: build application
cmake -DCMAKE_TOOLCHAIN_FILE="%QT_TOOLCHAIN%" ..\chap
cmake --build . --config MinSizeRel

:: prep for windeployqt
del /f /q MinSizeRel\*.lib
del /f /q MinSizeRel\*.exp

:: bring in dependencies with windeployqt and zip it all up
del /f /q MinSizeRel\*.pdb
del /f /q MinSizeRel\*.lib
del /f /q MinSizeRel\*.exp
del /f /q MinSizeRel\*.manifest
%QT_BIN%\windeployqt.exe ^
    --release ^
    --qmldir ..\chap ^
    MinSizeRel\chap.exe
..\tools\7za.exe a -tzip chap.zip .\MinSizeRel\*

cd /d %SCRIPT_DIR%
echo Done!
