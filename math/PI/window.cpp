#include "window.h"
#include <gl/gl.h>

HWND g_hwnd;
HDC g_hdc;

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
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "GLWindow";

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0,
        "GLWindow",
        title,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // Fixed size window
        CW_USEDEFAULT, 
        CW_USEDEFAULT,
        width, 
        height,
        NULL, 
        NULL,
        wc.hInstance, 
        NULL
    );

    if (!g_hwnd) return false;

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    SetWindowPos(g_hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);

    ShowWindow(g_hwnd, SW_SHOW);

    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(g_hdc, &pfd);
    SetPixelFormat(g_hdc, pf, &pfd);

    wglMakeCurrent(g_hdc, wglCreateContext(g_hdc));

    glEnable(GL_DEPTH_TEST);

    return true;
}

void ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) exit(0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
