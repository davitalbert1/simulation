#pragma once

#include <windows.h>

extern HWND g_hwnd;
extern HDC g_hdc;

bool CreateGLWindow(const char* title, int width, int height);
void ProcessMessages();
