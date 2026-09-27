#include "ui.h"
#include "optics.h"
#include "simtab.h"
#include <GL/gl.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

using std::min;
using std::max;

// Layout geral da interface
const float PANEL_X = 900.0f;
const float PANEL_W = 380.0f;

GLuint fontBaseRegular = 0;
GLuint fontBaseBold = 0;
GLuint fontBaseLarge = 0;

//  Fontes (mesmo esquema dos outros projetos: bitmaps gerados pelo Win32)
void BuildFonts() {
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
    HFONT fontLarge = CreateFont(-21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    SelectObject(g_hdc, fontLarge);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseLarge);

    SelectObject(g_hdc, oldFont);
    DeleteObject(fontReg);
    DeleteObject(fontBold);
    DeleteObject(fontLarge);
}

void PrintString(float x, float y, const char* str, GLuint base, float r, float g, float b) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

void DrawHelpCard(int mx, int my, bool clicked, UiState* ui);

//  Primitivas 2D
void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void DrawRectOutline(float x, float y, float w, float h, float r, float g, float b, float thickness) {
    glColor3f(r, g, b);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glLineWidth(1.0f);
}

void DrawLine2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float width) {
    glColor4f(r, g, b, a);
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
    glLineWidth(1.0f);
}

void DrawCircle2D(float cx, float cy, float radius, float r, float g, float b, float a, bool filled, int segments) {
    glColor4f(r, g, b, a);
    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (filled) glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float th = 2.0f * (float)M_PI * (float)i / (float)segments;
        glVertex2f(cx + cosf(th) * radius, cy + sinf(th) * radius);
    }
    glEnd();
}

void DrawTriangle2D(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void DrawArrow2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a, float head) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;

    float ux = dx / len;
    float uy = dy / len;

    DrawLine2D(x0, y0, x1, y1, r, g, b, a, 1.6f);

    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1);
    glVertex2f(x1 - ux * head - uy * head * 0.42f, y1 - uy * head + ux * head * 0.42f);
    glVertex2f(x1 - ux * head + uy * head * 0.42f, y1 - uy * head - ux * head * 0.42f);
    glEnd();
}

void DrawDashedLine2D(float x0, float y0, float x1, float y1, float r, float g, float b, float dash, float gap) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;

    float ux = dx / len;
    float uy = dy / len;
    float travelled = 0.0f;

    glColor4f(r, g, b, 0.85f);
    glLineWidth(1.2f);
    glBegin(GL_LINES);

    while (travelled < len) {
        float end = min(travelled + dash, len);
        glVertex2f(x0 + ux * travelled, y0 + uy * travelled);
        glVertex2f(x0 + ux * end, y0 + uy * end);
        travelled = end + gap;
    }

    glEnd();
    glLineWidth(1.0f);
}

//  Botoes e icones
bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    float r = 0.12f, g = 0.14f, b = 0.22f;
    if (active) {
        r = 0.16f;
        g = 0.40f;
        b = 0.90f;
    } else if (hovered) {
        r = 0.20f;
        g = 0.24f;
        b = 0.36f;
    }

    DrawRect(x, y, w, h, r, g, b, 0.92f);
    DrawRectOutline(x, y, w, h, 0.30f, 0.42f, 0.62f, 1.2f);

    float textW = strlen(label) * 7.0f;
    float tx = x + (w - textW) * 0.5f;
    float ty = y + (h - 10.0f) * 0.5f;

    if (tx < x + 6.0f) tx = x + 6.0f;

    PrintString(tx, ty, label, fontBaseBold, 1.0f, 1.0f);

    return hovered && clicked;
}

void DrawGlassIcon(GlassType type, float cx, float cy, float scale) {
    (void)scale;

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    float blue[3] = {0.55f, 0.80f, 0.98f};

    glColor4f(blue[0], blue[1], blue[2], 0.90f);
    glLineWidth(1.8f);

    switch (type) {
        case GT_WINDOW:
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - 27.0f, cy - 17.0f);
            glVertex2f(cx + 27.0f, cy - 17.0f);
            glVertex2f(cx + 27.0f, cy + 17.0f);
            glVertex2f(cx - 27.0f, cy + 17.0f);
            glEnd();
            DrawLine2D(cx - 27.0f, cy + 5.0f, cx - 8.0f, cy + 17.0f, blue[0], blue[1], blue[2], 0.35f, 1.2f);
            break;

        case GT_SHOWCASE:
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - 28.0f, cy - 14.0f);
            glVertex2f(cx + 28.0f, cy - 14.0f);
            glVertex2f(cx + 28.0f, cy + 14.0f);
            glVertex2f(cx - 28.0f, cy + 14.0f);
            glEnd();
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - 20.0f, cy - 14.0f);
            glVertex2f(cx - 20.0f, cy + 14.0f);
            glEnd();
            DrawLine2D(cx - 20.0f, cy - 14.0f, cx - 20.0f, cy + 14.0f, blue[0], blue[1], blue[2], 0.9f, 1.8f);
            DrawLine2D(cx - 26.0f, cy - 6.0f, cx - 14.0f, cy + 10.0f, blue[0], blue[1], blue[2], 0.28f, 1.2f);
            break;

        case GT_LENS:
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 40; i++) {
                float a = 2.0f * (float)M_PI * i / 40.0f;
                glVertex2f(cx + cosf(a) * 20.0f, cy + sinf(a) * 22.0f);
            }
            glEnd();
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 40; i++) {
                float a = 2.0f * (float)M_PI * i / 40.0f;
                glVertex2f(cx + cosf(a) * 27.0f, cy + sinf(a) * 22.0f);
            }
            glEnd();
            break;

        case GT_MAGNIFIER:
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 40; i++) {
                float a = 2.0f * (float)M_PI * i / 40.0f;
                glVertex2f(cx - 6.0f + cosf(a) * 15.0f, cy + 4.0f + sinf(a) * 15.0f);
            }
            glEnd();
            DrawLine2D(cx + 5.0f, cy - 7.0f, cx + 24.0f, cy - 22.0f, 0.45f, 0.36f, 0.26f, 1.0f, 4.0f);
            break;

        case GT_BOTTLE:
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - 12.0f, cy - 22.0f);
            glVertex2f(cx + 12.0f, cy - 22.0f);
            glVertex2f(cx + 12.0f, cy + 4.0f);
            glVertex2f(cx + 5.0f, cy + 14.0f);
            glVertex2f(cx + 5.0f, cy + 22.0f);
            glVertex2f(cx - 5.0f, cy + 22.0f);
            glVertex2f(cx - 5.0f, cy + 14.0f);
            glVertex2f(cx - 12.0f, cy + 4.0f);
            glEnd();
            DrawLine2D(cx - 5.0f, cy + 20.0f, cx + 5.0f, cy + 20.0f, 0.45f, 0.36f, 0.26f, 1.0f, 3.0f);
            break;

        case GT_PRISM:
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - 24.0f, cy - 18.0f);
            glVertex2f(cx + 24.0f, cy - 18.0f);
            glVertex2f(cx + 4.0f, cy + 20.0f);
            glEnd();
            break;

        default:
            break;
    }

    glPopAttrib();
    glLineWidth(1.0f);
}

//  Fundo do painel lateral
void DrawPanelBackground() {
    DrawRect(PANEL_X, 0.0f, PANEL_W, 690.0f, 0.075f, 0.085f, 0.12f, 1.0f);
    DrawRect(PANEL_X, 0.0f, 1.5f, 690.0f, 0.20f, 0.28f, 0.42f, 1.0f);
}

//  Cor por comprimento de onda (aproximacao usada em espectros)
void WavelengthColor(float nm, float* r, float* g, float* b) {
    float nr = 0.0f, ng = 0.0f, nb = 0.0f;

    if (nm >= 380.0f && nm < 440.0f) {
        nr = -(nm - 440.0f) / 60.0f;
        nb = 1.0f;
    } else if (nm < 490.0f) {
        ng = (nm - 440.0f) / 50.0f;
        nb = 1.0f;
    } else if (nm < 510.0f) {
        ng = 1.0f;
        nb = -(nm - 510.0f) / 20.0f;
    } else if (nm < 580.0f) {
        nr = (nm - 510.0f) / 70.0f;
        ng = 1.0f;
    } else if (nm < 645.0f) {
        nr = 1.0f;
        ng = -(nm - 645.0f) / 65.0f;
    } else {
        nr = 1.0f;
    }

    float atten = 1.0f;
    if (nm >= 380.0f && nm < 420.0f) atten = 0.3f + 0.7f * (nm - 380.0f) / 40.0f;
    else if (nm > 700.0f && nm <= 780.0f) atten = 0.3f + 0.7f * (780.0f - nm) / 80.0f;
    if (nm < 380.0f || nm > 780.0f) atten = 0.0f;

    *r = powf(nr * atten, 0.8f);
    *g = powf(ng * atten, 0.8f);
    *b = powf(nb * atten, 0.8f);
}

void SpectrumColor(int index, float* r, float* g, float* b) {
    static const float nm[] = {650.0f, 610.0f, 570.0f, 530.0f, 480.0f, 440.0f, 405.0f, 760.0f};
    int count = (int)(sizeof(nm) / sizeof(nm[0]));
    if (index < 0 || index >= count) index = 0;
    WavelengthColor(nm[index], r, g, b);
}

//  Help card da aba Simulacao
void DrawSimHelpCard(UiState* ui) {
    float w = 700.0f;
    float h = 320.0f;
    float x = 60.0f;
    float y = 250.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawRect(x, y, w, h, 0.05f, 0.06f, 0.11f, 0.92f);
    DrawRectOutline(x, y, w, h, 0.30f, 0.60f, 0.90f, 2.0f);
    glDisable(GL_BLEND);

    float tx = x + 24.0f;
    float ty = y + h - 34.0f;

    PrintString(tx + 60.0f, ty, "AJUDA E CONTROLES", fontBaseLarge, 0.35f, 0.72f, 1.00f);
    ty -= 30.0f;

    PrintString(tx, ty, "[ Colocar vidros ]", fontBaseBold, 0.90f, 0.90f, 1.00f);
    PrintString(tx + 15.0f, ty - 16.0f, "- Escolha um modelo na biblioteca 'Adicionar vidro' (painel a direita).", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 32.0f, "- Clique sobre a bancada e arraste ate o ponto desejado para soltar a peca.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 48.0f, "- Teclas 1 a 6 colocam janela, vitrine, lente, lupa, garrafa e prisma.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    ty -= 68.0f;

    PrintString(tx, ty, "[ Mover o vidro ]", fontBaseBold, 0.90f, 0.90f, 1.00f);
    PrintString(tx + 15.0f, ty - 16.0f, "- Clique em uma peca da cena e arraste: ela acompanha o cursor sobre a mesa.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 32.0f, "- Setas movem em X/Z, W/S alteram a altura, A/D giram e Q/E mudam o indice de refracao.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 48.0f, "- Del remove a peca selecionada | R restaura a montagem inicial.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    ty -= 68.0f;

    PrintString(tx, ty, "[ Camera e luz ]", fontBaseBold, 0.90f, 0.90f, 1.00f);
    PrintString(tx + 15.0f, ty - 16.0f, "- Botao DIREITO do mouse orbita a camera; a roda aproxima e afasta.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 32.0f, "- Selecione uma fonte de luz e mova com Setas/WASD: veja a refracao mudar.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(tx + 15.0f, ty - 48.0f, "- T, C, L e G ligam e desligam raios, vidros, fontes e grade | H fecha esta ajuda.", fontBaseRegular, 0.78f, 0.82f, 0.92f);

    (void)ui;
}

//  Help card da aba Modelos
void DrawModelsHelpCard(UiState* ui) {
    float w = 420.0f;
    float h = 232.0f;
    float x = 30.0f;
    float y = 40.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawRect(x, y, w, h, 0.05f, 0.06f, 0.11f, 0.92f);
    DrawRectOutline(x, y, w, h, 0.30f, 0.60f, 0.90f, 2.0f);
    glDisable(GL_BLEND);

    PrintString(x + 20.0f, y + h - 30.0f, "COMO LER O DIAGRAMA", fontBaseBold, 0.35f, 0.72f, 1.00f);

    PrintString(x + 20.0f, y + h - 56.0f, "Seta amarela: objeto luminoso a esquerda do vidro.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(x + 20.0f, y + h - 76.0f, "Linhas amarelas: tres raios principais que definem o foco.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(x + 20.0f, y + h - 96.0f, "Verde: a imagem formada pelo vidro (ponto de cruzamento).", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(x + 20.0f, y + h - 116.0f, "Tracejado azul: distancia focal e eixo optico.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(x + 20.0f, y + h - 136.0f, "Vermelho espesso: o vidro, com raio de curvatura e espessura reais.", fontBaseRegular, 0.78f, 0.82f, 0.92f);
    PrintString(x + 20.0f, y + h - 160.0f, "No prisma o tracado e refletivo e o raio se refrata nas duas faces.", fontBaseRegular, 0.62f, 0.68f, 0.80f);
    PrintString(x + 20.0f, y + h - 180.0f, "Use as abas para alternar o modelo e o menu para trocar o material.", fontBaseRegular, 0.62f, 0.68f, 0.80f);

    (void)ui;
}

//  Barra superior
void DrawTopBar(const char* leftLabel, const char* rightLabel, int* activeTab, int mx, int my, bool clicked,
                UiState* ui, const char* title, const char* subtitle) {
    (void)leftLabel;
    (void)rightLabel;

    DrawRect(0.0f, 690.0f, 1280.0f, 110.0f, 0.045f, 0.050f, 0.075f, 1.0f);
    DrawRect(0.0f, 690.0f, 1280.0f, 1.5f, 0.22f, 0.30f, 0.46f, 1.0f);

    PrintString(28.0f, 758.0f, title, fontBaseLarge, 0.90f, 0.95f, 1.00f);
    PrintString(28.0f, 736.0f, subtitle, fontBaseRegular, 0.55f, 0.62f, 0.74f);

    bool simActive = activeTab && *activeTab == 0;
    if (DrawButton(940.0f, 726.0f, 150.0f, 34.0f, simActive ? "1 - SIMULACAO" : "1 - SIMULACAO",
                   simActive, mx, my, clicked)) {
        *activeTab = 0;
        if (ui) ui->dirty = true;
    }

    bool modelActive = activeTab && *activeTab == 1;
    if (DrawButton(1100.0f, 726.0f, 152.0f, 34.0f, modelActive ? "2 - MODELOS" : "2 - MODELOS",
                   modelActive, mx, my, clicked)) {
        *activeTab = 1;
        if (ui) ui->dirty = true;
    }
}