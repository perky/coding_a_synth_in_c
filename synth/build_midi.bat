tcc -o midi.exe midi.c -Iinclude -lmsvcrt -lgdi32 -lkernel32 -lshell32 -luser32 -lwinmm -Wl -std=c99 -g