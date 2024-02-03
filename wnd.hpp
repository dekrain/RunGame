#pragma once

#include <GL/gl.h>

#include "event.hpp"

struct InitParams {
    GLint gl_major;
    GLint gl_minor;
    uint32_t stencil_size;
    uint32_t init_width, init_height;
    char const *title;
};

struct WindowState;

extern WindowState* init_window(InitParams const& init);
extern void make_current(WindowState* window);
extern void window_finish(WindowState* window);
extern void window_swap(WindowState* window);
extern bool window_pop_event(WindowState* window, WinEvent& event);
