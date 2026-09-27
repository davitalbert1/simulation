#include "window.h"
#include "camera.h"
#include "ui.h"
#include "optics.h"
#include "simtab.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <vector>

using std::min;
using std::max;

static Camera cam;
static UiState ui;

static bool draggingCamera = false;
static int lastMouseX = 0;
static int lastMouseY = 0;
static float wheelAccum = 0.0f;

static bool prevKeys[256] = {false};
static bool helpToggleWasDown = false;

static const float PANEL_TOP = 690.0f;

//  Teclado: dispara os eventos de "tecla pressionada agora" (sem repeticao)
static void HandleKeyboard(float dt) {
    for (int vk = 0; vk < 256; vk++) {
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool pressedNow = down && !prevKeys[vk];
        prevKeys[vk] = down;

        if (!down) continue;

        if (vk == 'H') {
            if (pressedNow) ui.helpVisible = !ui.helpVisible;
            continue;
        }

        if (vk == VK_TAB) {
            if (pressedNow) {
                ui.activeTab = 1 - ui.activeTab;
                ui.dirty = true;
            }
            continue;
        }

        if (ui.activeTab == 0) {
            SimulationTabHandleKey(vk, dt, pressedNow, down);
        } else {
            ModelsTabHandleKey(vk, dt, pressedNow);
        }
    }
}

//  Interacao com a camera (botao direito orbita, roda aproxima)
static void HandleCameraInput() {
    POINT p;
    GetCursorPos(&p);

    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
        if (!draggingCamera) {
            lastMouseX = p.x;
            lastMouseY = p.y;
            draggingCamera = true;
        } else {
            cam.yaw += (p.x - lastMouseX) * 0.28f;
            cam.pitch += (p.y - lastMouseY) * 0.28f;
            lastMouseX = p.x;
            lastMouseY = p.y;
        }
    } else {
        draggingCamera = false;
    }

    cam.pitch = min(max(cam.pitch, -78.0f), 80.0f);
    cam.distance = min(max(cam.distance, 2.6f), 22.0f);
}

//  HUD 2D (tambem informa ao estado se o cursor esta sobre o painel)
static void DrawHUD(int mx, int my, bool clicked, bool mouseDown) {
    glViewport(0, 0, g_width, g_height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)g_width, 0.0, (double)g_height, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawPanelBackground();

    // O painel ocupa toda a faixa direita: o clique nao deve arrastar o vidro.
    ui.panelHover = (mx >= PANEL_X);

    if (ui.activeTab == 0) {
        DrawSimTabPanel(mx, my, clicked, mouseDown, &ui);
    } else {
        DrawModelsPanel(mx, my, clicked, mouseDown, &ui);
    }

    DrawTopBar("", "", &ui.activeTab, mx, my, clicked, &ui,
               "REFRACAO E REFLEXAO NO VIDRO",
               "Bancada optica interativa: janelas, lentes, lupas, garrafas e prismas");

    if (ui.helpVisible) {
        if (ui.activeTab == 0) {
            DrawSimHelpCard(&ui);
        } else {
            DrawModelsHelpCard(&ui);
        }
    }

    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_CULL_FACE);
}

//  Programa principal
int main() {
    if (!CreateGLWindow("Refracao e Reflexao no Vidro", 1280, 800)) {
        std::puts("Erro ao criar a janela OpenGL.");
        return 1;
    }

    BuildFonts();

    glClearColor(0.020f, 0.024f, 0.038f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    ResetSimulation();

    std::puts("Controles:");
    std::puts("  Botao esquerdo + arrastar: mover vidros sobre a bancada");
    std::puts("  Botao direito + arrastar : orbitar a camera");
    std::puts("  Roda do mouse: aproximar / afastar");
    std::puts("  Setas / W A S D: mover a peca ou a fonte de luz");
    std::puts("  Q / E: diminuir / aumentar o indice de refracao");
    std::puts("  1 a 6: adicionar janela, vitrine, lente, lupa, garrafa ou prisma");
    std::puts("  T, C, L, G: raios, vidros, fontes e grade");
    std::puts("  Del: remover a peca selecionada");
    std::puts("  R: restaurar a montagem inicial");
    std::puts("  H: mostrar / ocultar a ajuda");
    std::puts("  Tab: alternar entre Simulacao e Modelos");

    DWORD lastTick = GetTickCount();

    while (true) {
        ProcessMessages();

        DWORD now = GetTickCount();
        float dt = (now - lastTick) * 0.001f;
        lastTick = now;
        if (dt > 0.1f) dt = 0.1f;

        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(g_hwnd, &mousePos);

        int mx = mousePos.x;
        int my = g_height - mousePos.y;

        static bool lastMouseState = false;
        bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = mouseDown && !lastMouseState;
        lastMouseState = mouseDown;

        ui.mouseDown = mouseDown;
        ui.mouseClicked = clicked;

        HandleCameraInput();
        HandleKeyboard(dt);

        if (ui.activeTab == 0) {
            SimulationTabInput(mx, my, clicked, mouseDown, &ui, dt);
        } else {
            ModelsTabInput(mx, my, clicked, mouseDown, &ui, dt);
        }

        if (ui.activeTab == 0 && gAutoRotate) cam.yaw += 12.0f * dt;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // cena 3D (aba simulacao) ou diagrama 2D (aba modelos)
        if (ui.activeTab == 0) {
            glViewport(0, 0, (int)PANEL_X, g_height);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(52.0, (double)PANEL_X / (double)g_height, 0.05, 120.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            ApplyCamera(cam);

            DrawSimulationScene();
        } else {
            glViewport(0, 0, g_width, g_height);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0.0, (double)g_width, 0.0, (double)g_height, -1.0, 1.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            DrawModelsScene();
            glEnable(GL_LIGHTING);
            glEnable(GL_DEPTH_TEST);
        }

        // HUD
        DrawHUD(mx, my, clicked, mouseDown);

        // O HUD deixa o estado "limpo" para o proximo quadro.
        ui.draggingIcon = ui.draggingIcon && mouseDown;
        ui.dragging = ui.dragging && mouseDown;
        ui.dirty = false;

        char title[200];
        std::snprintf(title, sizeof(title),
                      "Refracao e Reflexao no Vidro | %s | vidros: %d | FPS: %.0f",
                      ui.activeTab == 0 ? "Simulacao 3D" : "Modelos",
                      (int)gGlasses.size(),
                      dt > 0.0001f ? 1.0f / dt : 0.0f);
        SetWindowText(g_hwnd, title);

        SwapBuffers(g_hdc);
    }

    return 0;
}