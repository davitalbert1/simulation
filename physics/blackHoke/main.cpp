#include "window.h"
#include "camera.h"
#include "render.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>
#include <cstdio>
#include <algorithm>

static Camera cam;
static Vec3 lightPos = {3.2f, 2.0f, 2.6f};
static bool dragging = false;
static int lastX = 0;
static int lastY = 0;

static void PrintControls() {
    std::puts("Controles:");
    std::puts("  Mouse esquerdo + arrastar : orbitar a camera");
    std::puts("  Setas ou W/S : aproximar/afastar camera");
    std::puts("  A/D : girar camera");
    std::puts("  Q/E : subir/descer camera");
    std::puts("  J/L : mover luz em X");
    std::puts("  U/O : mover luz em Y");
    std::puts("  I/K : mover luz em Z");
    std::puts("  R : reset camera e luz");
}

static void UpdateInput(float dt) {
    float cameraSpeed = 65.0f * dt;
    float zoomSpeed = 4.0f * dt;
    float lightSpeed = 3.0f * dt;

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        POINT p;
        GetCursorPos(&p);

        if (!dragging) {
            lastX = p.x;
            lastY = p.y;
            dragging = true;
        }

        cam.yaw += (p.x - lastX) * 0.24f;
        cam.pitch += (p.y - lastY) * 0.24f;
        lastX = p.x;
        lastY = p.y;
    } else {
        dragging = false;
    }

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) cam.yaw -= cameraSpeed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) cam.yaw += cameraSpeed;
    if (GetAsyncKeyState('A') & 0x8000) cam.yaw -= cameraSpeed;
    if (GetAsyncKeyState('D') & 0x8000) cam.yaw += cameraSpeed;
    if (GetAsyncKeyState('Q') & 0x8000) cam.pitch += cameraSpeed * 0.5f;
    if (GetAsyncKeyState('E') & 0x8000) cam.pitch -= cameraSpeed * 0.5f;

    if (GetAsyncKeyState(VK_UP) & 0x8000) cam.distance -= zoomSpeed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) cam.distance += zoomSpeed;
    if (GetAsyncKeyState('W') & 0x8000) cam.distance -= zoomSpeed;
    if (GetAsyncKeyState('S') & 0x8000) cam.distance += zoomSpeed;

    if (GetAsyncKeyState('J') & 0x8000) lightPos.x -= lightSpeed;
    if (GetAsyncKeyState('L') & 0x8000) lightPos.x += lightSpeed;
    if (GetAsyncKeyState('U') & 0x8000) lightPos.y += lightSpeed;
    if (GetAsyncKeyState('O') & 0x8000) lightPos.y -= lightSpeed;
    if (GetAsyncKeyState('I') & 0x8000) lightPos.z -= lightSpeed;
    if (GetAsyncKeyState('K') & 0x8000) lightPos.z += lightSpeed;

    if (GetAsyncKeyState('R') & 0x8000) {
        cam = Camera();
        lightPos = {3.2f, 2.0f, 2.6f};
    }

    cam.pitch = std::clamp(cam.pitch, -82.0f, 82.0f);
    cam.distance = std::clamp(cam.distance, 2.4f, 34.0f);
    lightPos.x = std::clamp(lightPos.x, -8.0f, 8.0f);
    lightPos.y = std::clamp(lightPos.y, -5.0f, 7.0f);
    lightPos.z = std::clamp(lightPos.z, -8.0f, 8.0f);
}

int main() {
    if (!CreateGLWindow("Buraco negro", 1100, 720)) {
        std::puts("Erro ao criar janela OpenGL.");
        return 1;
    }

    PrintControls();

    glClearColor(0.005f, 0.006f, 0.012f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    InitScene();

    DWORD lastTick = GetTickCount();
    DWORD startTick = lastTick;

    while (true) {
        ProcessMessages();

        DWORD now = GetTickCount();
        float dt = (now - lastTick) * 0.001f;
        float time = (now - startTick) * 0.001f;
        lastTick = now;

        UpdateInput(dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, 1100.0 / 720.0, 0.05, 120.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        ApplyCamera(cam);

        DrawBlackHoleScene(time, lightPos);

        char title[180];
        std::snprintf(title, sizeof(title), "Buraco negro", lightPos.x, lightPos.y, lightPos.z, cam.distance);
        SetWindowText(g_hwnd, title);

        SwapBuffers(g_hdc);
    }
}
