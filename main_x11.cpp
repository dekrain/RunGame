#include <iostream>
#include <cstring>

#include <X11/Xlib.h>

#include <GL/glx.h>
#include <GL/glu.h>

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

#ifdef DISPLAY_X11
int main() {
    Display* xdsp = XOpenDisplay(nullptr);
    if (!xdsp) {
        std::cerr << "Couldn't open X display\n";
        return 1;
    }

    Window wnd_root = DefaultRootWindow(xdsp);
    static GLint attrs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(xdsp, 0, attrs);
    if (!vi) {
        std::cerr << "No appropiate visual found\n";
        return 1;
    }

    Colormap cmap = XCreateColormap(xdsp, wnd_root, vi->visual, AllocNone);

    XSetWindowAttributes xswa;
    memset(&xswa, 0, sizeof(XSetWindowAttributes));
    xswa.colormap = cmap;
    xswa.event_mask = ExposureMask | KeyPressMask;

    Window wnd = XCreateWindow(xdsp, wnd_root, 0, 0, 600, 400, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &xswa);
    XMapWindow(xdsp, wnd);
    XStoreName(xdsp, wnd, "Test ogl");

    GLXContext gctx = glXCreateContext(xdsp, vi, nullptr, GL_TRUE);
    // Activate the context
    glXMakeCurrent(xdsp, wnd, gctx);

    glEnable(GL_DEPTH_TEST);

    // Main loop
    XEvent ev;
    while (true) {
        XNextEvent(xdsp, &ev);
        switch (ev.type) {
            case Expose: {
                XWindowAttributes xwa;
                XGetWindowAttributes(xdsp, wnd, &xwa);
                // Render
                glViewport(0, 0, xwa.width, xwa.height);
                test_draw_quad();
                glXSwapBuffers(xdsp, wnd);
                break;
            }
            case KeyPress: {
                goto end_prog;
            }
            default:;
        }
    }
    end_prog:
    glXMakeCurrent(xdsp, None, nullptr);
    glXDestroyContext(xdsp, gctx);
    XDestroyWindow(xdsp, wnd);
    XCloseDisplay(xdsp);
}
#endif
