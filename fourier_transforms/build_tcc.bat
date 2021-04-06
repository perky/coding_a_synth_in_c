pushd bin
call tcc -o main.exe ../src/main.c -I../include -L../lib -lraylib -lmsvcrt -lopengl32 -lgdi32 -lkernel32 -lshell32 -luser32 -lwinmm -Wl,-subsystem=gui -std=c99
popd