#include "window.h"
#include "simulation.h"
#include "render.h"
#include "camera.h"
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdlib>

bool isRunning = true;
bool showHelpCard = true;
int simulationSpeed = 1; // 1x, 2x, 4x
Camera camera;

// FUNÇÃO PRINCIPAL
int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    if (!CreateGLWindow("Simulacao de Evolucao", 800, 600)) return 1;

    InitRender();
    SetupProjection(800, 600);

    ResetSimulation();

    bool rKeyDown = false;
    bool hKeyDown = false;
    bool spaceKeyDown = false;
    bool upKeyDown = false;
    bool downKeyDown = false;

    int lastMouseX = 0, lastMouseY = 0;
    bool isDragging = false;

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (true) {
        ProcessMessages();

        // Leitura de Teclado
        if (GetAsyncKeyState(VK_UP) & 0x8000) camera.distance -= 0.5f;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) camera.distance += 0.5f;
        if (camera.distance < 1.0f) camera.distance = 1.0f;


        // Teclas de Controle
        if (GetAsyncKeyState('R')) {
            if (!rKeyDown) {
                ResetSimulation();
                rKeyDown = true;
            }
        } else {
            rKeyDown = false;
        }

        bool hPressed = (GetAsyncKeyState('H') & 0x8000) != 0;
        if (hPressed && !hKeyDown) showHelpCard = !showHelpCard;
        hKeyDown = hPressed;

        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spacePressed && !spaceKeyDown) isRunning = !isRunning;
        spaceKeyDown = spacePressed;

        // Controle de Velocidade (Setas Direita/Esquerda)
        if (GetAsyncKeyState(VK_RIGHT)) {
            if (!upKeyDown) { // Reusing upKeyDown logic for right arrow
                if (simulationSpeed < 16) simulationSpeed *= 2;
                upKeyDown = true;
            }
        } else {
            upKeyDown = false;
        }

        if (GetAsyncKeyState(VK_LEFT)) {
            if (!downKeyDown) { // Reusing downKeyDown logic for left arrow
                if (simulationSpeed > 1) simulationSpeed /= 2;
                downKeyDown = true;
            }
        } else {
            downKeyDown = false;
        }

        // Controle do Mouse para Câmera
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(g_hwnd, &p);

            if (!isDragging) {
                lastMouseX = p.x;
                lastMouseY = p.y;
                isDragging = true;
            }
            camera.yaw += (p.x - lastMouseX) * 0.25f;
            camera.pitch += (p.y - lastMouseY) * 0.25f;
            lastMouseX = p.x;
            lastMouseY = p.y;
        } else {
            isDragging = false;
        }

        // Atualização com Delta Time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Limita o dt para evitar saltos grandes se a janela for movida, etc.
        if (dt > 0.1f) dt = 0.1f;

        if (isRunning) {
            for (int i = 0; i < simulationSpeed; ++i) UpdateSimulation(dt);
        }

        DrawGLScene();

        SwapBuffers(g_hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}