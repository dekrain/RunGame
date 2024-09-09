#include <SDL2/SDL.h>

#include "wnd.hpp"

struct WindowState {
    SDL_Window* window;
    SDL_GLContext gl_context;
    SDL_Event event;
};

WindowState* init_window(InitParams const& init) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, init.gl_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, init.gl_minor);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, init.stencil_size);

    SDL_Window* wnd = SDL_CreateWindow(init.title, 0, 0, init.init_width, init.init_height, SDL_WINDOW_OPENGL);
    SDL_GLContext gctx = SDL_GL_CreateContext(wnd);

    return new WindowState {
        .window = wnd,
        .gl_context = gctx,
    };
}

void make_current(WindowState* window) {
    SDL_GL_MakeCurrent(window->window, window->gl_context);
}

void window_finish(WindowState* window) {
    SDL_GL_DeleteContext(window->gl_context);
    SDL_DestroyWindow(window->window);

    SDL_Quit();
    delete window;
}

void window_swap(WindowState* window) {
    SDL_GL_SwapWindow(window->window);
}

bool window_pop_event(WindowState* window, WinEvent& event) {
    next:
    if (!SDL_PollEvent(&window->event))
        return false;
    switch (window->event.type) {
        case SDL_QUIT:
            event.type = EventType::Quit;
            break;
        case SDL_KEYDOWN:
            event.type = EventType::KeyDown;
            break;
        case SDL_KEYUP:
            event.type = EventType::KeyUp;
            break;
        default:
            goto next;
    }
    if (event.type == EventType::KeyDown or event.type == EventType::KeyUp) {
        #define COMMON_KEYS \
            MK(1, N1) \
            MK(2, N2) \
            MK(3, N3) \
            MK(4, N4) \
            MK(5, N5) \
            MK(6, N6) \
            MK(7, N7) \
            MK(8, N8) \
            MK(9, N9) \
            MK(0, N0) \
            MK(RETURN, Return) \
            MK(ESCAPE, Escape) \
            MK(BACKSPACE, Backspace) \
            MK(TAB, Tab) \
            MK(SPACE, Space) \
            MK(LEFT, ArrowLeft) \
            MK(RIGHT, ArrowRight) \
            MK(UP, ArrowUp) \
            MK(DOWN, ArrowDown) \
            MK(PAGEUP, PageUp) \
            MK(PAGEDOWN, PageDown) \
            MK(INSERT, Insert) \
            MK(DELETE, Delete) \
            // End
        // Map key
        #define MK(sdl, enum) case SDL_SCANCODE_##sdl: event.key.pkey = PhysicalKey::enum; break;
        switch (window->event.key.keysym.scancode) {
            MK(A, A)
            MK(B, B)
            MK(C, C)
            MK(D, D)
            MK(E, E)
            MK(F, F)
            MK(G, G)
            MK(H, H)
            MK(I, I)
            MK(J, J)
            MK(K, K)
            MK(L, L)
            MK(M, M)
            MK(N, N)
            MK(O, O)
            MK(P, P)
            MK(Q, Q)
            MK(R, R)
            MK(S, S)
            MK(T, T)
            MK(U, U)
            MK(V, V)
            MK(W, W)
            MK(X, X)
            MK(Y, Y)
            MK(Z, Z)
            COMMON_KEYS
            MK(MINUS, Minus)
            MK(EQUALS, Equals)
            default:
                event.key.pkey = PhysicalKey::Unknown;
        }
        #undef MK
        #define MK(sdl, enum) case SDLK_##sdl: event.key.lkey = LogicalKey::enum; break;
        switch (window->event.key.keysym.sym) {
            MK(a, A)
            MK(b, B)
            MK(c, C)
            MK(d, D)
            MK(e, E)
            MK(f, F)
            MK(g, G)
            MK(h, H)
            MK(i, I)
            MK(j, J)
            MK(k, K)
            MK(l, L)
            MK(m, M)
            MK(n, N)
            MK(o, O)
            MK(p, P)
            MK(q, Q)
            MK(r, R)
            MK(s, S)
            MK(t, T)
            MK(u, U)
            MK(v, V)
            MK(w, W)
            MK(x, X)
            MK(y, Y)
            MK(z, Z)
            COMMON_KEYS
            MK(PLUS, Plus)
            MK(MINUS, Minus)
            MK(EQUALS, Equals)
            default:
                event.key.lkey = LogicalKey::Unknown;
        }
        #undef MK
    }
    return true;
}
