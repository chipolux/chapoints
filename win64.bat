@echo off
set SCRIPT_DIR=%~dp0
set VC_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build
set QT_BIN=C:\Qt\6.4.2\msvc2019_64\bin
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
%QT_BIN%\windeployqt.exe ^
    --release ^
    --no-compiler-runtime ^
    --no-virtualkeyboard ^
    --no-translations ^
    --no-bluetooth ^
    --no-concurrent ^
    --no-designer ^
    --no-designercomponents ^
    --no-gamepad ^
    --no-qthelp ^
    --no-multimedia ^
    --no-multimediawidgets ^
    --no-multimediaquick ^
    --no-nfc ^
    --no-openglwidgets ^
    --no-positioning ^
    --no-printsupport ^
    --no-qmltooling ^
    --no-quickparticles ^
    --no-quickwidgets ^
    --no-script ^
    --no-scripttools ^
    --no-sensors ^
    --no-serialport ^
    --no-sql ^
    --no-svgwidgets ^
    --no-test ^
    --no-widgets ^
    --no-winextras ^
    --no-webenginecore ^
    --no-webengine ^
    --no-webenginewidgets ^
    --no-3dcore ^
    --no-3drenderer ^
    --no-3dquick ^
    --no-3dquickrenderer ^
    --no-3dinput ^
    --no-3danimation ^
    --no-3dextras ^
    --no-geoservices ^
    --no-webchannel ^
    --no-texttospeech ^
    --no-serialbus ^
    --no-webview ^
    --no-shadertools ^
    --qmldir ..\chap ^
    MinSizeRel\chap.exe
..\tools\7za.exe a -tzip chap.zip .\MinSizeRel\*

:: Qt libraries can be added by passing their name (-xml) or removed by passing
:: the name prepended by --no- (--no-xml). Available libraries:
:: bluetooth concurrent core declarative designer designercomponents gamepad gui
:: qthelp multimedia multimediawidgets multimediaquick network nfc opengl
:: openglwidgets positioning printsupport qml qmltooling quick quickparticles
:: quickwidgets script scripttools sensors serialport sql svg svgwidgets test
:: websockets widgets winextras xml webenginecore webengine webenginewidgets 3dcore
:: 3drenderer 3dquick 3dquickrenderer 3dinput 3danimation 3dextras geoservices
:: webchannel texttospeech serialbus webview shadertools

cd /d %SCRIPT_DIR%
echo Done!
