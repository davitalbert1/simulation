#include "window.h"
#include <GL/gl.h>
#include <cstdlib>

HWND g_hwnd = nullptr;
HDC g_hdc = nullptr;
static HGLRC g_glrc = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool CreateGLWindow(const char* title, int width, int height) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "BlackHoleGLWindow";
    wc.style = CS_OWNDC;

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    if (!g_hwnd) return false;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cAlphaBits = 8;

    int pixelFormat = ChoosePixelFormat(g_hdc, &pfd);
    if (!pixelFormat) return false;

    if (!SetPixelFormat(g_hdc, pixelFormat, &pfd)) return false;

    g_glrc = wglCreateContext(g_hdc);
    if (!g_glrc) return false;

    if (!wglMakeCurrent(g_hdc, g_glrc)) return false;

    glEnable(GL_DEPTH_TEST);
    return true;
}

void ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            std::exit(0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
