@echo off

cl /nologo /MT /W4 /EHsc /O2 /utf-8 /DUNICODE /D_UNICODE ^
   main.cpp LineacEngine.cpp BindManager.cpp WindowSelector.cpp ^
   /Fe:LineCord.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED user32.lib gdi32.lib comctl32.lib winmm.lib
if errorlevel 1 goto :error

echo.
echo Build succeeded: LineCord.exe
del /q *.obj 2>nul
goto :eof

:error
echo.
echo Build FAILED.
exit /b 1
