#!/bin/sh
set -e

CC="${CC-cc}"
CXX="${CXX-c++}"

OBJS=()
BACKEND="${BACKEND-sdl}"

case "$BACKEND" in
    sdl)
        OBJS+=(wnd_sdl.cpp -lSDL2)
        ;;
    xlib)
        echo 'WARNING: X11/Xlib backend is in experimental stage'
        OBJS+=(wnd_xlib.cpp -lX11)
        ;;
    xcb)
        echo 'XCB backend is TODO' >&2
        exit 1;
        ;;
    wayland)
        echo 'Wayland backend is TODO' >&2
        exit 1;
        ;;
    *)
        echo "Unknown backend: $BACKEND" >&2
        exit 1;
        ;;
esac

if [ ! -f glew.o ]; then
    echo 'Compiling GLEW'
    cc -c -o glew.o -I "$GLEW_PATH/include" "$GLEW_PATH/src/glew.c" -DGLEW_STATIC
fi
c++ -DGLEW_STATIC -std=c++20 -o run glew.o main.cpp util.cpp run.cpp -lGL "${OBJS[@]}"
