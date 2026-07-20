#include "window.h"
#include "camera.h"
#include "render.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <cstring>

static Camera cam;
static Vec3 lightPos = {3.2f, 2.0f, 2.6f};
static bool dragging = false;
static int lastX = 0;
static int lastY = 0;

static GLuint fontBaseRegular = 0;
static GLuint fontBaseBold = 0;
static GLuint fontBaseLarge = 0;
static bool showHelpCard = true;

// O HUD utiliza uma projeção paralela ortográfica bidimensional (2D) para a renderização de elementos textuais e painéis sobre a viewport 3D.
// As fontes tipográficas são geradas por bitmaps a partir da API Win32 do Windows.
static void BuildFonts() {
    fontBaseRegular = glGenLists(96);
    HFONT fontReg = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(g_hdc, fontReg);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseRegular);

    fontBaseBold = glGenLists(96);
    HFONT fontBold = CreateFont(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    SelectObject(g_hdc, fontBold);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseBold);

    fontBaseLarge = glGenLists(96);
    HFONT fontLarge = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    SelectObject(g_hdc, fontLarge);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseLarge);

    SelectObject(g_hdc, oldFont);
    DeleteObject(fontReg);
    DeleteObject(fontBold);
    DeleteObject(fontLarge);
}

static void PrintString(float x, float y, const char* str, GLuint base, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)std::strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

static void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawRectOutline(float x, float y, float w, float h, float r, float g, float b, float thickness = 1.0f) {
    glColor3f(r, g, b);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    float r = 0.12f, g = 0.14f, b = 0.22f;
    if (active) {
        r = 0.16f; g = 0.40f; b = 0.90f;
    } else if (hovered) {
        r = 0.20f; g = 0.23f; b = 0.35f;
    }

    DrawRect(x, y, w, h, r, g, b, 0.85f);
    DrawRectOutline(x, y, w, h, 0.3f, 0.4f, 0.6f);

    int labelLen = (int)std::strlen(label);
    float textW = labelLen * 7.0f;
    float tx = x + (w - textW) / 2.0f;
    float ty = y + (h - 10.0f) / 2.0f;

    PrintString(tx, ty, label, fontBaseBold, 1.0f, 1.0f, 1.0f);

    return hovered && clicked;
}

static void DrawHelpCardHUD(float cx, float cy) {
    float w = 420.0f;
    float h = 300.0f;
    float cardX = cx - w / 2.0f;
    float cardY = cy - h / 2.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawRect(cardX, cardY, w, h, 0.05f, 0.06f, 0.11f, 0.90f);
    DrawRectOutline(cardX, cardY, w, h, 0.3f, 0.6f, 0.9f, 2.0f);
    glDisable(GL_BLEND);

    float tx = cardX + 20.0f;
    float ty = cardY + h - 30.0f;
    PrintString(tx + 65.0f, ty, "AJUDA E CONTROLES", fontBaseLarge, 0.3f, 0.7f, 1.0f);
    ty -= 30.0f;

    PrintString(tx, ty, "[ Camera ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- Arrastar mouse : orbitar camera.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 32.0f, "- Setas ou W/S : aproximar / afastar (zoom).", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 48.0f, "- A/D : girar a camera lateralmente.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 64.0f, "- Q/E : elevar / abaixar a camera.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    ty -= 85.0f;

    PrintString(tx, ty, "[ Luz (Fonte de Brilho) ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- J/L : mover em X | U/O : mover em Y.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 32.0f, "- I/K : mover em Z.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    ty -= 50.0f;

    PrintString(tx, ty, "[ Atalhos ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- H : Ocultar / Mostrar este card de ajuda.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 32.0f, "- R : Reiniciar camera e posicao da luz.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
}

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

    POINT mousePos;
    GetCursorPos(&mousePos);
    POINT clientP = mousePos;
    ScreenToClient(g_hwnd, &clientP);
    int mx = clientP.x;
    int my = 720 - clientP.y;

    bool overButton = (mx >= 20 && mx <= 140 && my >= 668 && my <= 700);
    bool overCard = showHelpCard && (mx >= 20 && mx <= 440 && my >= 330 && my <= 630);

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        POINT p;
        GetCursorPos(&p);

        if (!dragging) {
            if (!overButton && !overCard) {
                lastX = p.x;
                lastY = p.y;
                dragging = true;
            }
        }

        if (dragging) {
            cam.yaw += (p.x - lastX) * 0.24f;
            cam.pitch += (p.y - lastY) * 0.24f;
            lastX = p.x;
            lastY = p.y;
        }
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

    BuildFonts();
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
    bool hKeyPressedLast = false;

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

        // RENDERIZAR INTERFACE HUD (2D ORTOGRÁFICA)
        glViewport(0, 0, 1100, 720);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, 1100, 0, 720, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);

        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(g_hwnd, &mousePos);
        int mx = mousePos.x;
        int my = 720 - mousePos.y;

        static bool lastMouseState = false;
        bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = currentMouseState && !lastMouseState;
        lastMouseState = currentMouseState;

        bool hKeyPressedNow = (GetAsyncKeyState('H') & 0x8000) != 0;
        if (hKeyPressedNow && !hKeyPressedLast) showHelpCard = !showHelpCard;
        hKeyPressedLast = hKeyPressedNow;

        // Botão interativo para exibir ou ocultar os controles de ajuda
        if (DrawButton(20, 668, 120, 32, showHelpCard ? "Fechar Ajuda" : "Ajuda (H)", false, mx, my, clicked)) {
            showHelpCard = !showHelpCard;
        }

        if (showHelpCard) DrawHelpCardHUD(230.0f, 480.0f);

        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        char title[180];
        std::snprintf(title, sizeof(title), "Buraco negro", lightPos.x, lightPos.y, lightPos.z, cam.distance);
        SetWindowText(g_hwnd, title);

        SwapBuffers(g_hdc);
    }
}
