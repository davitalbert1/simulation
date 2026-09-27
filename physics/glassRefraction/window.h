#pragma once
#include <windows.h>

extern HWND g_hwnd;
extern HDC g_hdc;
extern int g_width;
extern int g_height;

bool CreateGLWindow(const char* title, int width, int height);
void ProcessMessages();