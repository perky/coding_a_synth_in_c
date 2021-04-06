SET VS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools
CALL "%VS%\VsDevCmd.bat" -arch=x64

SET CommonFlags=-nologo -Od -Oi /I"include" /Zi /Gm- /Gd /TC
SET CommonLinkerFlags=raylib.lib -opt:ref kernel32.lib user32.lib gdi32.lib shell32.lib winmm.lib
cl.exe %CommonFlags% synth.c /link /OUT:"synth.exe" /LIBPATH:"lib/vs2019"  %CommonLinkerFlags%