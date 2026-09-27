#include "ui.h"
#include "optics.h"
#include "simtab.h"
#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

using std::min;
using std::max;

//  Estado da aba
int gModelType = GT_LENS;
int gModelMassIndex = 0; // 0 = vidro leve (crown), 1 = vidro pesado (flint)

static float gModelHeight = 0.62f;// altura do objeto sobre o eixo
static float gModelDistance = 3.40f; // distancia do objeto ao vidro

static const float DIAGRAM_Y = 390.0f;  // linha do eixo optico na tela
static const float DIAGRAM_SCALE = 105.0f;
static const float GLASS_CENTER_X = 420.0f;

// Material do modelo: primeiro e ultimo vidros do catalogo do sistema.
static float ModelIor() {
    return MaterialIor(gModelMassIndex == 0 ? 0 : 1, 1);
}

//  Informacoes do modelo (numeros de serie)
ModelInfo ComputeModelInfo(int type, float ior, float height) {
    ModelInfo info;
    info.ior = ior;

    float width = 1.30f;
    float thickness = 0.55f;

    switch (type) {
        case GT_WINDOW:
            width = 1.90f; thickness = 0.10f;
            break;
        case GT_SHOWCASE:
            width = 1.60f; thickness = 0.48f;
            break;
        case GT_LENS:
            width = 1.30f; thickness = 0.55f;
            break;
        case GT_MAGNIFIER:
            width = 1.80f; thickness = 0.90f;
            break;
        case GT_BOTTLE:
            width = 1.05f; thickness = 1.05f;
            break;
        case GT_PRISM:
            width = 1.15f; thickness = 1.35f;
            break;
        default:
            break;
    }

    float radius = max(thickness * 0.5f, 0.02f);
    float edge = sqrtf(max(radius * radius - (width * 0.5f) * (width * 0.5f), 0.0f));

    info.radius = radius;
    info.curvature = 1.0f / radius;

    // Superficie mais espessa que a lente: 1/f = (n - 1) * (1/r1 - 1/r2).
    float edgeThickness = max(2.0f * (radius - edge), 0.005f);
    float r1 = radius;
    float r2 = -radius;
    info.focal = 1.0f / ((ior - 1.0f) * (1.0f / r1 - 1.0f / r2));

    // Numero f: quanto o vidro e rapido em relacao ao seu diametro.
    info.fNumber = (width > 0.01f) ? info.focal / width : 0.0f;

    // Formula de Gauss: 1/f = 1/do + 1/di.
    float objectDistance = gModelDistance;
    info.objectDistance = objectDistance;

    if (objectDistance <= info.focal + 1e-4f) {
        // Objeto entre o foco e o vidro: imagem virtual, do mesmo lado.
        float denom = 1.0f / info.focal - 1.0f / objectDistance;
        if (fabsf(denom) < 1e-5f) denom = 1e-5f;
        info.imageDistance = -1.0f / denom;
        info.imageKind = "virtual";
    } else {
        info.imageDistance = 1.0f / (1.0f / info.focal - 1.0f / objectDistance);
        info.imageKind = "real";
    }

    info.magnification = -info.imageDistance / objectDistance;
    info.imageHeight = info.magnification * height;

    // O desenho usa o raio meridional do sistema, mais fiel que a formula fina.
    GlassObject g;
    InitGlass(g, (GlassType)type, 0.0f, 0.0f, 0.0f);
    g.ior = ior;
    g.width = width;
    g.thickness = thickness;

    GlassParams params;
    params.glasses.push_back(g);

    // O tracado de duas superficies precisa de uma segunda peca; aqui o modelo
    // e analisado pela formula de Gauss e pelo desenho direto das superficies.
    (void)edgeThickness;

    return info;
}

//  Desenho do vidro em corte meridional
static void DrawGlassCrossSection(const GlassObject& g, float radius) {
    float cx = GLASS_CENTER_X;
    float cy = DIAGRAM_Y;
    float s = DIAGRAM_SCALE;

    glColor4f(0.35f, 0.72f, 0.95f, 0.20f);

    if (g.type == GT_PRISM) {
        DrawTriangle2D(cx - 0.52f * s, cy - 0.62f * s,
                       cx + 0.52f * s, cy - 0.62f * s,
                       cx + 0.10f * s, cy + 0.72f * s,
                       0.35f, 0.72f, 0.95f, 0.22f);

        DrawLine2D(cx - 0.52f * s, cy - 0.62f * s, cx + 0.52f * s, cy - 0.62f * s, 0.55f, 0.85f, 1.0f, 1.0f, 2.0f);
        DrawLine2D(cx + 0.52f * s, cy - 0.62f * s, cx + 0.10f * s, cy + 0.72f * s, 0.55f, 0.85f, 1.0f, 1.0f, 2.0f);
        DrawLine2D(cx + 0.10f * s, cy + 0.72f * s, cx - 0.52f * s, cy - 0.62f * s, 0.55f, 0.85f, 1.0f, 1.0f, 2.0f);
        return;
    }

    if (g.type == GT_WINDOW || g.type == GT_SHOWCASE) {
        float halfT = radius * s;
        float halfW = g.width * 0.5f * s;

        DrawRect(cx - halfT, cy - halfW, halfT * 2.0f, halfW * 2.0f, 0.35f, 0.72f, 0.95f, 0.22f);
        DrawRectOutline(cx - halfT, cy - halfW, halfT * 2.0f, halfW * 2.0f, 0.55f, 0.85f, 1.0f, 2.0f);
        return;
    }

    // Lentes, lupa e garrafa: as duas calotas esfericas de raio r.
    float r = radius * s;
    float halfOpen = min(g.width * 0.5f * s, r * 0.995f);
    float yEdge = sqrtf(max(r * r - halfOpen * halfOpen, 0.0f));

    glColor4f(0.35f, 0.72f, 0.95f, 0.18f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 40; i++) {
        float t = (float)i / 40.0f;
        float y = -halfOpen + t * 2.0f * halfOpen;
        float x = sqrtf(max(r * r - y * y, 0.0f));
        glVertex2f(cx - r + x, cy + y);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 40; i++) {
        float t = (float)i / 40.0f;
        float y = halfOpen - t * 2.0f * halfOpen;
        float x = sqrtf(max(r * r - y * y, 0.0f));
        glVertex2f(cx + r - x, cy + y);
    }
    glEnd();

    glColor3f(0.55f, 0.85f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 40; i++) {
        float t = (float)i / 40.0f;
        float y = -halfOpen + t * 2.0f * halfOpen;
        float x = sqrtf(max(r * r - y * y, 0.0f));
        glVertex2f(cx - r + x, cy + y);
    }
    glEnd();

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 40; i++) {
        float t = (float)i / 40.0f;
        float y = halfOpen - t * 2.0f * halfOpen;
        float x = sqrtf(max(r * r - y * y, 0.0f));
        glVertex2f(cx + r - x, cy + y);
    }
    glEnd();

    glLineWidth(1.0f);
    (void)yEdge;
}

// ---------------------------------------------------------------------------
//  Diagrama principal
// ---------------------------------------------------------------------------
void DrawModelsScene() {
    ModelInfo info = ComputeModelInfo(gModelType, ModelIor(), gModelHeight);

    GlassObject g;
    InitGlass(g, (GlassType)gModelType, 0.0f, 0.0f, 0.0);
    g.ior = ModelIor();

    float s = DIAGRAM_SCALE;
    float cx = GLASS_CENTER_X;
    float cy = DIAGRAM_Y;
    float radius = max(g.thickness * 0.5f, 0.02f);

    // Fundo do diagrama
    DrawRect(20.0f, 20.0f, 860.0f, 640.0f, 0.055f, 0.065f, 0.10f, 1.0f);
    DrawRectOutline(20.0f, 20.0f, 860.0f, 640.0f, 0.18f, 0.24f, 0.38f, 2.0f);

    PrintString(40.0f, 636.0f, "TRACADO MERIDIONAL: COMO O VIDRO DISTORCE A LUZ", fontBaseBold, 0.85f, 0.90f, 1.00f);

    // Eixo optico
    DrawDashedLine2D(40.0f, cy, 860.0f, cy, 0.35f, 0.55f, 0.85f, 8.0f, 6.0f);

    // O objeto fica a esquerda do vidro, sobre o eixo.
    float objectX = cx - info.objectDistance * s;
    float objectY = cy + gModelHeight * s;

    // Distancia focal (tracejado azul) para os dois lados
    float focalLeft = cx - info.focal * s;
    float focalRight = cx + info.focal * s;

    DrawDashedLine2D(focalLeft, cy - 60.0f, focalLeft, cy + 60.0f, 0.35f, 0.65f, 1.0f, 6.0f, 5.0f);
    DrawDashedLine2D(focalRight, cy - 60.0f, focalRight, cy + 60.0f, 0.35f, 0.65f, 1.0f, 6.0f, 5.0f);

    DrawCircle2D(focalLeft, cy, 3.0f, 0.40f, 0.70f, 1.0f, 1.0f, true, 16);
    DrawCircle2D(focalRight, cy, 3.0f, 0.40f, 0.70f, 1.0f, 1.0f, true, 16);

    PrintString(focalLeft - 14.0f, cy - 74.0f, "F", fontBaseBold, 0.45f, 0.72f, 1.00f);
    PrintString(focalRight - 4.0f, cy - 74.0f, "F'", fontBaseBold, 0.45f, 0.72f, 1.00f);

    // Objeto (seta amarela)
    DrawArrow2D(objectX, cy, objectX, objectY, 1.0f, 0.86f, 0.25f, 1.0f, 9.0f);
    DrawCircle2D(objectX, cy, 3.0f, 1.0f, 0.86f, 0.25f, 1.0f, true, 16);
    PrintString(objectX - 22.0f, objectY + 10.0f, "objeto", fontBaseRegular, 0.90f, 0.82f, 0.35f);

    // Vidro em corte
    DrawGlassCrossSection(g, radius);

    // Raios principais (aproximacao paraxial)
    float f = info.focal;
    float imageDistance = info.imageDistance;
    float imageHeight = info.imageHeight;
    float imageX = cx + imageDistance * s;
    float imageY = cy + imageHeight * s;

    // 1) raio paralelo ao eixo: refrata passando pelo foco imagem
    DrawLine2D(objectX, objectY, cx, objectY, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);
    DrawLine2D(cx, objectY, focalRight + (imageDistance - f) * s * 0.9f,
               cy + (imageHeight - gModelHeight) * s, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);

    // 2) raio pelo centro optico: nao desvia
    DrawLine2D(objectX, objectY, cx, cy, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);
    DrawLine2D(cx, cy, imageX, imageY, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);

    // 3) raio que passa pelo foco objeto: sai paralelo ao eixo
    DrawLine2D(objectX, objectY, focalLeft + (cx - focalLeft) * 0.5f, cy, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);
    DrawLine2D(focalLeft + (cx - focalLeft) * 0.5f, cy, cx, cy + gModelHeight * s * 0.35f, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);
    DrawLine2D(cx, cy + gModelHeight * s * 0.35f, imageX, cy + gModelHeight * s * 0.35f, 1.0f, 0.86f, 0.30f, 0.95f, 1.6f);

    // Imagem formada
    DrawArrow2D(imageX, cy, imageX, imageY, 0.30f, 0.95f, 0.45f, 1.0f, 9.0f);
    DrawCircle2D(imageX, cy, 3.0f, 0.30f, 0.95f, 0.45f, 1.0f, true, 16);
    PrintString(imageX - 24.0f, imageY - 20.0f, info.imageKind, fontBaseRegular, 0.45f, 0.95f, 0.55f);

    // Legendas de distancia
    char buf[128];
    snprintf(buf, sizeof(buf), "objeto  do = %.2f", info.objectDistance);
    PrintString(40.0f, 60.0f, buf, fontBaseRegular, 0.80f, 0.84f, 0.94f);

    snprintf(buf, sizeof(buf), "imagem  di = %.2f  (%s)", info.imageDistance, info.imageKind);
    PrintString(40.0f, 42.0f, buf, fontBaseRegular, 0.80f, 0.84f, 0.94f);

    snprintf(buf, sizeof(buf), "distancia focal  f = %.2f     aumento  A = %.2fx", info.focal, info.magnification);
    PrintString(40.0f, 24.0f, buf, fontBaseRegular, 0.80f, 0.84f, 0.94f);

    snprintf(buf, sizeof(buf), "numero f (f/D) = %.2f     espessura = %.2f     indice n = %.3f", info.fNumber, g.thickness, info.ior);
    PrintString(40.0f, 148.0f, buf, fontBaseRegular, 0.62f,0.68f, 0.80f);

    // Legenda do vidro
    PrintString(cx - 34.0f, cy + g.width * 0.5f * s + 16.0f, GlassTypeName((GlassType)gModelType), fontBaseBold, 0.55f, 0.85f, 1.00f);
}// ---------------------------------------------------------------------------
//  Painel da aba Modelos
// ---------------------------------------------------------------------------
void DrawModelsPanel(int mx, int my, bool clicked, bool mouseDown, UiState* ui) {
    (void)mouseDown;
    ui->nextSliderId = 1;

    float x = PANEL_X + 22.0f;
    float width = PANEL_W - 44.0f;
    float y = 250.0f;

    ModelInfo info = ComputeModelInfo(gModelType, ModelIor(), gModelHeight);
    char buf[160];

    PrintString(x, 764.0f, "MODELOS DE VIDRO", fontBaseLarge, 0.86f, 0.92f, 1.00f);
    PrintString(x, 744.0f, "Numeros de serie: foco, imagem e aumento", fontBaseRegular, 0.55f, 0.60f, 0.70f);

    // - escolha do modelo -----------------------------------------------------
    PrintString(x, y + 12.0f, "ESCOLHA O MODELO", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 12.0f;

    float cardW = 120.0f;
    float cardH = 94.0f;
    float gap = 9.0f;

    for (int t = 0; t < GT_TYPE_COUNT; t++) {
        int col = t % 3;
        int row = t / 3;

        float bx = x + col * (cardW + gap);
        float by = y - row * (cardH + gap) - cardH;
        bool active = (gModelType == t) && (row >= 0);
        bool hovered = (mx >= bx && mx <= bx + cardW && my >= by && my <= by + cardH);

        float r = 0.12f, g = 0.14f, b = 0.21f;
        if (gModelType == t) {
            r = 0.16f; g = 0.38f; b = 0.85f;
        } else if (hovered) {
            r = 0.17f; g = 0.23f; b = 0.35f;
        }

        DrawRect(bx, by, cardW, cardH, r, g, b, 0.95f);
        DrawRectOutline(bx, by, cardW, cardH, 0.30f, 0.42f, 0.62f, 1.5f);

        DrawGlassIcon((GlassType)t, bx + cardW * 0.5f, by + cardH - 38.0f, 1.0f);

        const char* name = GlassTypeName((GlassType)t);
        PrintString(bx + (cardW - strlen(name) * 7.0f) * 0.5f, by + 8.0f, name, fontBaseBold, 0.90f, 0.93f, 1.00f);

        if (hovered && clicked) {
            gModelType = t;
            ui->dirty = true;
        }

        (void)active;
    }

    y -= 2.0f * (cardH + gap) + 16.0f;

    // - material --------------------------------------------------------------
    PrintString(x, y, "MATERIAL DO VIDRO", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 30.0f;

    if (DrawButton(x, y, width, 28.0f, gModelMassIndex == 0 ? "Vidro leve (Crown, n ~ 1.52)" : "Vidro leve (Crown, n ~ 1.52)",
                   gModelMassIndex == 0, mx, my, clicked)) {
        gModelMassIndex = 0;
        ui->dirty = true;
    }
    y -= 34.0f;

    if (DrawButton(x, y, width, 28.0f, gModelMassIndex == 1 ? "Vidro pesado (Flint, n ~ 1.62)" : "Vidro pesado (Flint, n ~ 1.62)",
                   gModelMassIndex == 1, mx, my, clicked)) {
        gModelMassIndex = 1;
        ui->dirty = true;
    }
    y -= 44.0f;

    // - posicao do objeto -----------------------------------------------------
    PrintString(x, y, "POSICAO DO OBJETO", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 12.0f;

    PrintString(x, y, "Distancia do objeto ao vidro (do)", fontBaseRegular, 0.62f, 0.68f, 0.78f);
    float trackY = y - 12.0f;
    DrawRect(x, trackY - 3.0f, width, 6.0f, 0.17f, 0.19f, 0.27f, 1.0f);
    float t = (gModelDistance - 0.80f) / (8.0f - 0.80f);
    t = min(max(t, 0.0f), 1.0f);
    DrawRect(x, trackY - 3.0f, width * t, 6.0f, 0.24f, 0.58f, 0.95f, 1.0f);
    DrawRect(x + width * t - 3.5f, trackY - 8.0f, 7.0f, 16.0f, 0.86f, 0.93f, 1.00f, 1.0f);
    DrawRectOutline(x, trackY - 3.0f, width, 6.0f, 0.32f, 0.42f, 0.60f, 1.0f);

    snprintf(buf, sizeof(buf), "%.2f", gModelDistance);
    PrintString(x + width - strlen(buf) * 7.0f, y, buf, fontBaseBold, 0.86f, 0.92f, 1.00f);

    bool hoverTrack = (mx >= x - 4.0f && mx <= x + width + 4.0f && my >= trackY - 10.0f && my <= trackY + 10.0f);
    if (ui->sliderActive == 0 && hoverTrack && clicked) {
        ui->sliderActive = ui->nextSliderId;
    }
    if (ui->sliderActive == ui->nextSliderId && ui->mouseDown) {
        float nt = min(max((mx - x) / width, 0.0f), 1.0f);
        gModelDistance = 0.80f + nt * (8.0f - 0.80f);
        ui->dirty = true;
    } else if (ui->sliderActive == ui->nextSliderId) {
        ui->sliderActive = 0;
    }
    ui->nextSliderId++;

    y -= 44.0f;

    // - resultados ------------------------------------------------------------
    PrintString(x, y, "NUMEROS DE SERIE", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 24.0f;

    DrawRect(x, y - 196.0f, width, 204.0f, 0.09f, 0.10f, 0.15f, 1.0f);
    DrawRectOutline(x, y - 196.0f, width, 204.0f, 0.20f, 0.26f, 0.40f, 1.2f);

    float ty = y - 14.0f;

    snprintf(buf, sizeof(buf), "Indice de refracao  n = %.3f", info.ior);
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.82f, 0.86f, 0.96f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Raio da superficie  r = %.3f", info.radius);
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.82f, 0.86f, 0.96f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Curvatura  1/r = %.2f", info.curvature);
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.82f, 0.86f, 0.96f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Distancia focal  f = %.3f", info.focal);
    PrintString(x + 12.0f, ty, buf, fontBaseBold, 0.45f, 0.85f, 1.00f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Numero f (f/D) = %.2f", info.fNumber);
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.82f, 0.86f, 0.96f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Imagem  di = %.3f  (%s)", info.imageDistance, info.imageKind);
    PrintString(x + 12.0f, ty, buf, fontBaseBold, 0.35f, 0.95f, 0.55f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Altura da imagem  hi = %.3f", info.imageHeight);
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.82f, 0.86f, 0.96f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Aumento linear  A = %.2fx", info.magnification);
    PrintString(x + 12.0f, ty, buf, fontBaseBold, 0.98f, 0.84f, 0.45f);
    ty -= 22.0f;

    snprintf(buf, sizeof(buf), "Caminho otico da luz: entrada e saida");
    PrintString(x + 12.0f, ty, buf, fontBaseRegular, 0.60f, 0.66f, 0.78f);

    y -= 216.0f;

    if (DrawButton(x, y, width, 28.0f, "Restaurar medida padrao", false, mx, my, clicked)) {
        gModelDistance = 3.40f;
        gModelHeight = 0.62f;
        ui->dirty = true;
    }
}

// ---------------------------------------------------------------------------
//  Entrada da aba Modelos
// ---------------------------------------------------------------------------
void ModelsTabInput(int mx, int my, bool clicked, bool mouseDown, UiState* ui, float dt) {
    (void)mx; (void)my; (void)clicked; (void)mouseDown; (void)dt;

    if (ui->panelHover) {
        return;
    }
}

void ModelsTabHandleKey(int vk, float dt, bool pressedNow) {
    (void)dt;

    if (!pressedNow) return;

    switch (vk) {
        case '1': case '2': case '3': case '4': case '5': case '6':
            gModelType = vk - '1';
            return;
        case 'F':
            gModelMassIndex = 1 - gModelMassIndex;
            return;
        case 'R':
            gModelDistance = 3.40f;
            gModelHeight = 0.62f;
            return;
        default:
            break;
    }
}