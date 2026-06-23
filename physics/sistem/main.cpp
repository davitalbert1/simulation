#include "window.h"
#include "camera.h"
#include "render.h"
#include <gl/gl.h>
#include <windows.h>
#include <GL/glu.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <commdlg.h>

void DrawStars();
void InitStars(int count);

Camera cam;

int lastX, lastY;
bool dragging = false;
bool reloadKeyDown = false;
bool sKeyDown = false;
bool lKeyDown = false;
bool hKeyDown = false;
bool showHelpCard = true;
DWORD simulationStartTime = 0;

GLuint fontBaseRegular = 0;
GLuint fontBaseBold = 0;
GLuint fontBaseLarge = 0;

void BuildFontsHUD() {
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

void PrintStringHUD(float x, float y, const char* str, GLuint base, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

std::string ShowOpenFileDialog(HWND hwnd) {
    char filename[MAX_PATH] = "";
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Arquivos JSON (*.json)\0*.json\0Todos os arquivos (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileName(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string ShowSaveFileDialog(HWND hwnd) {
    char filename[MAX_PATH] = "";
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Arquivos JSON (*.json)\0*.json\0Todos os arquivos (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileName(&ofn)) {
        return std::string(filename);
    }
    return "";
}

void DrawHUD2D(int mx, int my, bool clicked) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 800, 0, 600, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    // Botão de Ajuda
    bool hoveredHelp = (mx >= 710 && mx <= 785 && my >= 550 && my <= 585);
    if (hoveredHelp && clicked) {
        showHelpCard = !showHelpCard;
    }
    
    glColor4f(0.12f, 0.14f, 0.22f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(710, 550);
    glVertex2f(785, 550);
    glVertex2f(785, 585);
    glVertex2f(710, 585);
    glEnd();
    
    glColor3f(0.3f, 0.6f, 0.9f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(710, 550);
    glVertex2f(785, 550);
    glVertex2f(785, 585);
    glVertex2f(710, 585);
    glEnd();
    
    PrintStringHUD(720, 562, showHelpCard ? "Fechar" : "Ajuda (H)", fontBaseBold, 1.0f, 1.0f, 1.0f);

    // Botão de Salvar
    bool hoveredSave = (mx >= 710 && mx <= 785 && my >= 505 && my <= 540);
    if (hoveredSave && clicked) {
        std::string path = ShowSaveFileDialog(g_hwnd);
        if (!path.empty()) SaveSystemToFile(path.c_str());
    }
    glColor4f(0.12f, 0.20f, 0.14f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(710, 505);
    glVertex2f(785, 505);
    glVertex2f(785, 540);
    glVertex2f(710, 540);
    glEnd();
    glColor3f(0.2f, 0.8f, 0.3f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(710, 505);
    glVertex2f(785, 505);
    glVertex2f(785, 540);
    glVertex2f(710, 540);
    glEnd();
    PrintStringHUD(725, 517, "Salvar (S)", fontBaseBold, 1.0f, 1.0f, 1.0f);

    // Botão de Carregar
    bool hoveredLoad = (mx >= 710 && mx <= 785 && my >= 460 && my <= 495);
    if (hoveredLoad && clicked) {
        std::string path = ShowOpenFileDialog(g_hwnd);
        if (!path.empty()) {
            if (LoadSystemFromFile(path.c_str())) {
                simulationStartTime = GetTickCount();
            }
        }
    }
    glColor4f(0.20f, 0.12f, 0.14f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(710, 460);
    glVertex2f(785, 460);
    glVertex2f(785, 495);
    glVertex2f(710, 495);
    glEnd();
    glColor3f(0.9f, 0.3f, 0.3f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(710, 460);
    glVertex2f(785, 460);
    glVertex2f(785, 495);
    glVertex2f(710, 495);
    glEnd();
    PrintStringHUD(718, 472, "Carregar (L)", fontBaseBold, 1.0f, 1.0f, 1.0f);

    // Card de Ajuda
    if (showHelpCard) {
        float w = 320.0f;
        float h = 240.0f;
        float cardX = 15.0f;
        float cardY = 345.0f;
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.05f, 0.06f, 0.11f, 0.90f);
        glBegin(GL_QUADS);
        glVertex2f(cardX, cardY);
        glVertex2f(cardX + w, cardY);
        glVertex2f(cardX + w, cardY + h);
        glVertex2f(cardX, cardY + h);
        glEnd();
        
        glColor3f(0.3f, 0.6f, 0.9f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(cardX, cardY);
        glVertex2f(cardX + w, cardY);
        glVertex2f(cardX + w, cardY + h);
        glVertex2f(cardX, cardY + h);
        glEnd();
        glDisable(GL_BLEND);

        float tx = cardX + 15.0f;
        float ty = cardY + h - 25.0f;
        PrintStringHUD(tx + 45.0f, ty, "CONTROLES DO SISTEMA", fontBaseLarge, 0.3f, 0.7f, 1.0f);
        ty -= 25.0f;

        PrintStringHUD(tx, ty, "[ Mouse ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
        PrintStringHUD(tx + 15.0f, ty - 14.0f, "- Arraste o botao esquerdo para rotacionar camera.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
        ty -= 35.0f;

        PrintStringHUD(tx, ty, "[ Teclado ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
        PrintStringHUD(tx + 15.0f, ty - 14.0f, "- Setas UP/DOWN : Controle de Zoom da camera.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
        PrintStringHUD(tx + 15.0f, ty - 28.0f, "- R : Re-gerar novo sistema estelar aleatorio.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
        PrintStringHUD(tx + 15.0f, ty - 42.0f, "- S : Salvar configuracao do sistema em JSON.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
        PrintStringHUD(tx + 15.0f, ty - 56.0f, "- L : Carregar configuracao de sistema de JSON.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
        PrintStringHUD(tx + 15.0f, ty - 70.0f, "- H : Mostrar / Ocultar este card de ajuda.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

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
    BuildFontsHUD();
    simulationStartTime = GetTickCount();

    while (true) {
        ProcessMessages();

        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(g_hwnd, &mousePos);
        int mx = mousePos.x;
        int my = 600 - mousePos.y; // 600 is height
        static bool lastMouseState = false;
        bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = currentMouseState && !lastMouseState;
        lastMouseState = currentMouseState;

        // input mouse para orbitar camera
        // Só arrasta se não clicar nos botões da direita
        bool onControlHUD = (mx >= 710 && mx <= 785 && my >= 460 && my <= 585);
        if (currentMouseState && !onControlHUD) {
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

        // Tecla S para salvar
        bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;
        if (sPressed && !sKeyDown) {
            std::string path = ShowSaveFileDialog(g_hwnd);
            if (!path.empty()) SaveSystemToFile(path.c_str());
        }
        sKeyDown = sPressed;

        // Tecla L para carregar
        bool lPressed = (GetAsyncKeyState('L') & 0x8000) != 0;
        if (lPressed && !lKeyDown) {
            std::string path = ShowOpenFileDialog(g_hwnd);
            if (!path.empty()) {
                if (LoadSystemFromFile(path.c_str())) {
                    simulationStartTime = GetTickCount();
                }
            }
        }
        lKeyDown = lPressed;

        // Tecla H para ajuda
        bool hPressed = (GetAsyncKeyState('H') & 0x8000) != 0;
        if (hPressed && !hKeyDown) {
            showHelpCard = !showHelpCard;
        }
        hKeyDown = hPressed;

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

        // Desenha a interface 2D por cima
        DrawHUD2D(mx, my, clicked);

        SwapBuffers(g_hdc);
    }

    return 0;
}
