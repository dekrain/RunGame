#!/bin/sh
set -e

CC="${CC-cc}"
CXX="${CXX-c++}"

if [ ! -f glew.o ]; then
    echo 'Compiling GLEW'
    cc -c -o glew.o -I "$GLEW_PATH/include" "$GLEW_PATH/src/glew.c" -DGLEW_STATIC
fi
c++ -DGLEW_STATIC -DDISPLAY_SDL -std=c++17 -o run glew.o main_sdl.cpp wnd_sdl.cpp util.cpp run.cpp -lGL -lSDL2
