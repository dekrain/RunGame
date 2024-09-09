#include <cstdio>
#include <cstring>
#include <memory>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <GL/glx.h>
#include <GL/glu.h>

//#define DEFINE_KEY_TO_STRING
#include "event.hpp"
#include "wnd.hpp"

enum class DisplayAtom {
    WM_Protocols,
    WM_DeleteWindow,

    _COUNT,
};

static constexpr size_t NUM_DISPLAY_ATOMS = static_cast<size_t>(DisplayAtom::_COUNT);

static char const* const sAtomList[NUM_DISPLAY_ATOMS] = {
    "WM_PROTOCOLS",
    "WM_DELETE_WINDOW",
};

struct DisplayAtomList {
    Atom atoms[NUM_DISPLAY_ATOMS];

    Atom const& operator[](DisplayAtom atom) const {
        return atoms[static_cast<size_t>(atom)];
    }

    operator Atom*() & {
        return atoms;
    }
};

#define K(n, s) {n, PhysicalKey::s},
static struct PhysicalKeyNameMapping {
    char name[XkbKeyNameLength];
    PhysicalKey key;

    consteval PhysicalKeyNameMapping(char const* name, PhysicalKey key) : name{}, key(key) {
        char* n = this->name;
        while (*name)
            *n++ = *name++;
    }
} sPhysKeys[] = {
    K("ESC", Escape)
    K("AB01", Z)
    K("AB02", X)
    K("AB03", C)
    K("AB04", V)
    K("AB05", B)
    K("AB06", N)
    K("AB07", M)
    //K("AB08", Comma)
    //K("AB09", Period)
    K("AC01", A)
    K("AC02", S)
    K("AC03", D)
    K("AC04", F)
    K("AC05", G)
    K("AC06", H)
    K("AC07", J)
    K("AC08", K)
    K("AC09", L)
    K("AD01", Q)
    K("AD02", W)
    K("AD03", E)
    K("AD04", R)
    K("AD05", T)
    K("AD06", Y)
    K("AD07", U)
    K("AD08", I)
    K("AD09", O)
    K("AD10", P)
    K("AE01", N1)
    K("AE02", N2)
    K("AE03", N3)
    K("AE04", N4)
    K("AE05", N5)
    K("AE06", N6)
    K("AE07", N7)
    K("AE08", N8)
    K("AE09", N9)
    K("AE10", N0)
    K("AE11", Minus)
    K("AE12", Equals)
    K("SPCE", Space)
    K("RTRN", Return)
    K("ESC", Escape)
    K("BKSP", Backspace)
    K("TAB", Tab)
    K("LEFT", ArrowLeft)
    K("RGHT", ArrowRight)
    K("UP", ArrowUp)
    K("DOWN", ArrowDown)
    K("PGUP", PageUp)
    K("PGDN", PageDown)
    K("INS", Insert)
    K("DELE", Delete)
};

struct PhysicalKeyMap {
    // Unknown is first, so it will init to Unknown
    PhysicalKey map[XkbMaxKeyCount] = {};
};


struct WindowState {
    Display* dpy;
    DisplayAtomList atoms;
    Window window;
    XkbDescPtr xkb_desc;
    PhysicalKeyMap phys_keys;
    GLXContext gl_ctx;
    XEvent event;
};

WindowState* init_window(InitParams const& init) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        std::fputs("Couldn't open X display\n", stderr);
        return nullptr;
    }

    std::unique_ptr<WindowState> ws(new WindowState);
    ws->dpy = dpy;

    if (!XInternAtoms(dpy, const_cast<char **>(sAtomList), NUM_DISPLAY_ATOMS, /* only_if_exists */ true, ws->atoms)) {
        std::fputs("Couldn't get server atoms\n", stderr);
        XCloseDisplay(dpy);
        return nullptr;
    }

    Window wnd_root = DefaultRootWindow(dpy);
    static GLint attrs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, (GLint)init.stencil_size,
        GLX_DOUBLEBUFFER,
        None,
    };
    XVisualInfo* vi = glXChooseVisual(dpy, 0, attrs);
    if (!vi) {
        std::fputs("No appropiate visual found\n", stderr);
        return nullptr;
    }

    Colormap cmap = XCreateColormap(dpy, wnd_root, vi->visual, AllocNone);

    XSetWindowAttributes xswa;
    memset(&xswa, 0, sizeof(XSetWindowAttributes));
    xswa.colormap = cmap;
    xswa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask;

    Window wnd = XCreateWindow(dpy, wnd_root, 0, 0, init.init_width, init.init_height, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &xswa);
    ws->window = wnd;
    XMapWindow(dpy, wnd);
    XStoreName(dpy, wnd, init.title);

    XSetWMProtocols(dpy, wnd, const_cast<Atom*>(&ws->atoms[DisplayAtom::WM_DeleteWindow]), 1);

    ws->xkb_desc = XkbGetMap(dpy, XkbAllMapComponentsMask, XkbUseCoreKbd);
    XkbGetNames(dpy, XkbKeyNamesMask, ws->xkb_desc);

    for (uint32_t kc = ws->xkb_desc->min_key_code; kc <= ws->xkb_desc->max_key_code; ++kc) {
        for (auto const& map : sPhysKeys) {
            if (std::strncmp(map.name, ws->xkb_desc->names->keys[kc].name, XkbKeyNameLength) == 0) {
                ws->phys_keys.map[kc - XkbMinLegalKeyCode] = map.key;
                break;
            }
        }
    }

    ws->gl_ctx = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
    return ws.release();
}

void make_current(WindowState* window) {
    glXMakeCurrent(window->dpy, window->window, window->gl_ctx);
}

void window_finish(WindowState* window_) {
    std::unique_ptr<WindowState> window(window_);
    glXMakeCurrent(window->dpy, None, nullptr);
    glXDestroyContext(window->dpy, window->gl_ctx);
    XkbFreeNames(window->xkb_desc, XkbKeyNamesMask, true);
    XkbFreeKeyboard(window->xkb_desc, XkbAllMapComponentsMask, true);
    XDestroyWindow(window->dpy, window->window);
    XCloseDisplay(window->dpy);
}

void window_swap(WindowState* window) {
    glXSwapBuffers(window->dpy, window->window);
}

static void get_x_key_codes(WindowState const& window, decltype(WinEvent::key)& key, uint32_t scancode, uint32_t mods) {
    if (scancode <= XkbMaxLegalKeyCode and scancode >= XkbMinLegalKeyCode) {
        key.pkey = window.phys_keys.map[scancode - XkbMinLegalKeyCode];
    } else {
        key.pkey = PhysicalKey::Unknown;
    }

    KeySym sym = NoSymbol;
    XkbTranslateKeyCode(window.xkb_desc, scancode, mods, /* ret_mods */ nullptr, &sym);
    #define SYM(xname, lname) case XK_##xname: key.lkey = LogicalKey::lname; break;
    #define AL(up, low) case XK_##up: case XK_##low: key.lkey = LogicalKey::up; break;
    #define BI(name) case XK_##name: key.lkey = LogicalKey::name; break;
    #define AR(name) case XK_##name: key.lkey = LogicalKey::Arrow##name; break;
    switch (sym) {
        // TODO: Handle this better
        AL(A, a) AL(B, b) AL(C, c) AL(D, d) AL(E, e) AL(F, f) AL(G, g) AL(H, h) AL(I, i) AL(J, j) AL(K, k) AL(L, l) AL(M, m) AL(N, n) AL(O, o) AL(P, p) AL(Q, q) AL(R, r) AL(S, s) AL(T, t) AL(U, u) AL(V, v) AL(W, w) AL(X, x) AL(Y, y) AL(Z, z)
        SYM(1, N1) SYM(2, N2) SYM(3, N3) SYM(4, N4) SYM(5, N5) SYM(6, N6) SYM(7, N7) SYM(8, N8) SYM(9, N9) SYM(0, N0)
        BI(Return)
        BI(Escape)
        SYM(BackSpace, Backspace)
        BI(Tab)
        SYM(space, Space)
        AR(Left)
        AR(Right)
        AR(Up)
        AR(Down)
        SYM(plus, Plus)
        SYM(minus, Minus)
        SYM(equal, Equals)
        SYM(Prior, PageUp)
        SYM(Next, PageDown)
        SYM(Insert, Insert)
        SYM(Delete, Delete)
        default:
            key.lkey = LogicalKey::Unknown;
            break;
    }
    #undef SYM
    #undef AL
    #undef BI
    #undef AR
}

bool window_pop_event(WindowState* window, WinEvent& event) {
    retry:
    #if 0
    if (XPending(window->dpy) <= 0) {
        return false;
    }
    XNextEvent(window->dpy, &window->event);
    #else
    if (!XCheckWindowEvent(window->dpy, window->window, ExposureMask | KeyPressMask | KeyReleaseMask, &window->event) and !XCheckTypedEvent(window->dpy, ClientMessage, &window->event)) {
        return false;
    }
    #endif
    switch (window->event.type) {
        case ClientMessage:
            if (window->event.xclient.message_type == window->atoms[DisplayAtom::WM_Protocols]) {
                if (static_cast<Atom>(window->event.xclient.data.l[0]) == window->atoms[DisplayAtom::WM_DeleteWindow]) {
                    event.type = EventType::Quit;
                    break;
                }
            }
            goto retry;
        case KeyPress:
            event.type = EventType::KeyDown;
            get_x_key_codes(*window, event.key, window->event.xkey.keycode, window->event.xkey.state);
            break;
        case KeyRelease:
            event.type = EventType::KeyUp;
            get_x_key_codes(*window, event.key, window->event.xkey.keycode, window->event.xkey.state);
            break;
        default: goto retry;
    }
    #if 0
    switch (event.type) {
        case EventType::Quit:
            std::puts("[X11 Debug] Event: Quit");
            break;
        case EventType::KeyDown:
            std::printf("[X11 Debug] Event: KeyDown: logical=%s physical=%s\n", LogicalKeyAsString(event.key.lkey), PhysicalKeyAsString(event.key.pkey));
            break;
        case EventType::KeyUp:
            std::printf("[X11 Debug] Event: KeyUp: logical=%s physical=%s\n", LogicalKeyAsString(event.key.lkey), PhysicalKeyAsString(event.key.pkey));
            break;
    }
    #endif
    return true;
}


#if 0
static void test_draw_quad() {
    // Clear screen
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1., 1., -1., 1., 1., 20.);

    // Set object matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.);

    glBegin(GL_TRIANGLE_STRIP);
        glColor3f(1., 0., 0.); glVertex3f(-.75, -.75, 0.);
        glColor3f(1., 1., 0.); glVertex3f(-.75,  .75, 0.);
        glColor3f(0., 1., 0.); glVertex3f( .75, -.75, 0.);
        glColor3f(0., 0., 1.); glVertex3f( .75,  .75, 0.);
    glEnd();
}

static int example_main(WindowState &state) {
    glEnable(GL_DEPTH_TEST);

    // Main loop
    XEvent event;
    while (true) {
        XNextEvent(state.window, &state.event);
        switch (event.type) {
            case Expose: {
                XWindowAttributes xwa;
                XGetWindowAttributes(state.dpy, state.window, &xwa);
                // Render
                glViewport(0, 0, xwa.width, xwa.height);
                test_draw_quad();
                glXSwapBuffers(state.dpy, state.window);
                break;
            }
            case KeyPress: {
                goto end_prog;
            }
            default:;
        }
    }
    end_prog:
    window_finish(&state);
}
#endif
