#include "window.h"
#include "camera.h"
#include "render.h"
#include <gl/gl.h>
#include <windows.h>
#include <GL/glu.h>
#include <cstdlib>
#include <ctime>

void DrawStars();
void InitStars(int count);

Camera cam;

int lastX, lastY;
bool dragging = false;
bool reloadKeyDown = false;
DWORD simulationStartTime = 0;

void ReloadSolarSystem() {
    srand((unsigned)time(nullptr) ^ GetTickCount());
    InitSuns();
    InitPlanets(10);
    InitStars(2000);
}

int main() {
    CreateGLWindow("Solar System", 800, 600);

    glClearColor(0.02f, 0.01f, 0.04f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_COLOR_MATERIAL);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_POINT_SMOOTH);
    glPointSize(1.2f);

    InitPlanetTextures();
    ReloadSolarSystem();
    simulationStartTime = GetTickCount();

    while (true) {
        ProcessMessages();

        // input mouse
        if (GetAsyncKeyState(VK_LBUTTON)) {
            POINT p;
            GetCursorPos(&p);

            if (!dragging) {
                lastX = p.x;
                lastY = p.y;
                dragging = true;
            }

            cam.angleY += (p.x - lastX) * 0.2f;
            cam.angleX += (p.y - lastY) * 0.2f;

            lastX = p.x;
            lastY = p.y;
        }
        else {
            dragging = false;
        }

        // teclas
        if (GetAsyncKeyState(VK_UP)) cam.distance -= 0.1f;
        if (GetAsyncKeyState(VK_DOWN)) cam.distance += 0.1f;

        bool reloadPressed = (GetAsyncKeyState('R') & 0x8000) != 0;
        if (reloadPressed && !reloadKeyDown) {
            ReloadSolarSystem();
            simulationStartTime = GetTickCount();
        }
        reloadKeyDown = reloadPressed;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        gluPerspective(60, 800.0 / 600.0, 0.1, 100);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        ApplyCamera(cam);

        DrawStars();

        float time = (GetTickCount() - simulationStartTime) * 0.001f;

        DrawSolarSystem(time);

        SwapBuffers(g_hdc);
    }

    return 0;
}
