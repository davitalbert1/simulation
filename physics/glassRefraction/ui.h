#pragma once

#include "window.h"
#include "optics.h"
#include <vector>
#include <GL/gl.h>

// Modo de posicionamento usado pela biblioteca de vidros do painel.
enum PlacementMode {
    PM_DRAG = 0,    // arrastar o modelo do painel e soltar sobre a mesa
    PM_CLICK = 1    // clicar no modelo para adicionar no centro da mesa
};

// Layout geral da interface (painel lateral direito).
extern const float PANEL_X;
extern const float PANEL_W;

//  Elementos de interface compartilhados pelas duas abas.
extern GLuint fontBaseRegular;
extern GLuint fontBaseBold;
extern GLuint fontBaseLarge;

void BuildFonts();
void PrintString(float x, float y, const char* str, GLuint base, float r = 1.0f, float g = 1.0f, float b = 1.0f);

void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
void DrawRectOutline(float x, float y, float w, float h, float r, float g, float b, float thickness = 1.0f);
void DrawLine2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a = 1.0f, float width = 1.0f);
void DrawCircle2D(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, bool filled = false, int segments = 48);
void DrawTriangle2D(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a = 1.0f);
void DrawArrow2D(float x0, float y0, float x1, float y1, float r, float g, float b, float a = 1.0f, float head = 8.0f);
void DrawDashedLine2D(float x0, float y0, float x1, float y1, float r, float g, float b, float dash = 7.0f, float gap = 5.0f);

bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked);

// Icone em miniatura de cada tipo de vidro (usado na biblioteca do painel).
void DrawGlassIcon(GlassType type, float cx, float cy, float scale);

//  Estado de interacao acumulado entre o HUD (que desenha) e a cena 3D (que
//  consome os cliques).  O HUD e desenhado primeiro para informar se o cursor
//  esta sobre o painel e onde a peca arrastada deve cair.
struct UiState {
    bool mouseDown = false;
    bool mouseClicked = false;
    bool mouseRightDown = false;

    bool panelHover = false;
    bool dragging = false;
    bool draggingIcon = false;
    int dragType = 0;
    bool dropValid = false;
    float dropX = 0.0f;
    float dropZ = 0.0f;

    int placement = 0;      // uso interno: 0 = arrastar e soltar, 1 = clique
    int activeTab = 0;
    bool helpVisible = true;

    int nextSliderId = 1;
    int sliderActive = 0;

    bool dirty = true;
};

// Painel lateral direito, presente nas duas abas.
void DrawPanelBackground();

// Barra superior com titulo e alternancia entre as abas.
void DrawTopBar(const char* leftLabel, const char* rightLabel, int* activeTab, int mx, int my, bool clicked,
                UiState* ui, const char* title, const char* subtitle);

// Card de ajuda (conteudo de cada aba).
void DrawSimHelpCard(UiState* ui);
void DrawModelsHelpCard(UiState* ui);

//  Aba simulacao
void AddGlass(int type, float x, float z);
void RemoveGlass(int index);
void ClearGlasses();
void ResetSimulation();
void DragSelectedGlass(float targetX, float targetZ);
void MoveSelectedGlass(float dx, float dy);
void MoveSelectedLight(float dx, float dy);

// Estado compartilhado da bancada (definido em simtab.cpp).
extern bool gShowRays;
extern bool gShowObjects;
extern bool gShowLightGizmo;
extern bool gAutoRotate;
extern int gSelectedGlass;
extern int gSelectedLight;
extern std::vector<GlassObject> gGlasses;
extern std::vector<LightSource> gLights;

// Caminho de um raio tracado, com a cor da fonte de luz que o originou.
struct RayPath {
    std::vector<Vec3> points;
    float color[3] = {1.0f, 1.0f};
};

void BuildRayPaths(std::vector<RayPath>& out);
void DrawSimulationScene();
void DrawSimTabPanel(int mx, int my, bool clicked, bool mouseDown, UiState* ui);
void SimulationTabInput(int mx, int my, bool clicked, bool mouseDown, UiState* ui, float dt);
void SimulationTabHandleKey(int vk, float dt, bool pressedNow, bool held);

//  Aba modelos (tracado bidimensional meridional)
void DrawModelsScene();
void DrawModelsPanel(int mx, int my, bool clicked, bool mouseDown, UiState* ui);
void ModelsTabInput(int mx, int my, bool clicked, bool mouseDown, UiState* ui, float dt);
void ModelsTabHandleKey(int vk, float dt, bool pressedNow);

extern int gModelType; // tipo de vidro em analise
extern int gModelMassIndex; // 0 = vidro leve, 1 = vidro pesado (flint)

// Dados do tracado meridional exibidos nos paineis.
struct ModelInfo {
    float radius = 0.0f;
    float focal = 0.0f;
    float fNumber = 0.0f;
    float curvature = 0.0f;
    float objectDistance = 0.0f;
    float imageDistance = 0.0f;
    float imageHeight = 0.0f;
    float magnification = 0.0f;
    float ior = 1.52f;
    const char* imageKind = "real";
};

ModelInfo ComputeModelInfo(int type, float ior, float height);

//  Cores por comprimento de onda (dispersao)
void WavelengthColor(float nm, float* r, float* g, float* b);
void SpectrumColor(int index, float* r, float* g, float* b);