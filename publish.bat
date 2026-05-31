@echo off
:: Build native DLL
cd HookDLL
cmake -B build -G "MinGW Makefiles"
if %errorlevel% neq 0 exit /b %errorlevel%
cmake --build build --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
if not exist ..\DeskPins\Resources mkdir ..\DeskPins\Resources
copy build\DeskPinsHook.dll ..\DeskPins\Resources\
cd ..

:: Publish single-file EXE
dotnet publish DeskPins\DeskPins.csproj -c Release ^
  -p:PublishSingleFile=true ^
  -p:SelfContained=false ^
  -p:PublishTrimmed=true ^
  -o publish
if %errorlevel% neq 0 exit /b %errorlevel%

echo Done: publish\DeskPins.exe
