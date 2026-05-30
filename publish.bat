@echo off
:: Build native DLL
cd HookDLL
cmake -B build -G "MinGW Makefiles"
cmake --build build --config Release
copy build\DeskPinsHook.dll ..\DeskPins\Resources\
cd ..

:: Publish single-file EXE
dotnet publish DeskPins\DeskPins.csproj -c Release ^
  -p:PublishSingleFile=true ^
  -p:SelfContained=false ^
  -p:PublishTrimmed=true ^
  -o publish

echo Done: publish\DeskPins.exe
