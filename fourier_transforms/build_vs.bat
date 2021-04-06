@echo off
SET VS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools
if not defined DevEnvDir (
    CALL "%VS%\VsDevCmd.bat" -arch=x64
)

pushd bin
SET CommonFlags=-nologo -Od -Oi /I"..\include" /Zi /Gm- /Gd /TC
SET CommonLinkerFlags=/LIBPATH:"..\lib\vs2019" raylib.lib -opt:ref kernel32.lib user32.lib gdi32.lib shell32.lib winmm.lib
call cl.exe %CommonFlags% "..\src\main.c" /link /OUT:"main.exe"  %CommonLinkerFlags%
popd