#include "simtab.h"
#include "optics.h"
#include "ui.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

using std::min;
using std::max;

//  Estado global da simulacao
std::vector<GlassObject> gGlasses;
std::vector<LightSource> gLights;
int gSelectedGlass = -1;
int gSelectedLight = 0;
bool gShowRays = true;
bool gShowGrid = true;
bool gShowObjects = true;
bool gShowLightGizmo = true;
bool gAutoRotate = false;
bool gTraceDirty = true;
float gTime = 0.0f;

Camera* gCamera = nullptr;
bool gHelpVisible = true;

static const float kSphereRadius = 0.55f;

//  Utilitarios
Vec3 MakeVec(float x, float y, float z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

Vec3 Add(const Vec3& a, const Vec3& b) {
    return MakeVec(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 Sub(const Vec3& a, const Vec3& b) {
    return MakeVec(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 Scale(const Vec3& v, float s) {
    return MakeVec(v.x * s, v.y * s, v.z * s);
}

float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return MakeVec(a.y * b.z - a.z * b.y,
                   a.z * b.x - a.x * b.z,
                   a.x * b.y - a.y * b.x);
}

float Length(const Vec3& v) {
    return sqrtf(Dot(v, v));
}

Vec3 Normalize(const Vec3& v) {
    float l = Length(v);
    if (l < 1e-6f) return MakeVec(0.0f, 1.0f, 0.0f);
    return Scale(v, 1.0f / l);
}

float Clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}

//  Colisao (clique)
// Ponto da mesa correspondente ao clique.
static bool HitGlass(const GlassObject& g, int mx, int my, float* outX, float* outZ) {
    float px = 0.0f, pz = 0.0f;
    if (!ScreenToTable(mx, my, &px, &pz)) return false;

    float dx = px - g.x;
    float dz = pz - g.z;
    float reach = max(g.width, g.thickness) * 0.5f + 0.30f;

    if (dx * dx + dz * dz > reach * reach) return false;

    *outX = px;
    *outZ = pz;
    return true;
}

static int PickGlassAt(int mx, int my, float* outX, float* outZ) {
    int best = -1;
    float bestDist = 1e9f;

    for (size_t i = 0; i < gGlasses.size(); i++) {
        float px = 0.0f, pz = 0.0f;
        if (!HitGlass(gGlasses[i], mx, my, &px, &pz)) continue;

        float d = (px - gGlasses[i].x) * (px - gGlasses[i].x) + (pz - gGlasses[i].z) * (pz - gGlasses[i].z);
        if (d < bestDist) {
            bestDist = d;
            best = (int)i;
            if (outX) *outX = px;
            if (outZ) *outZ = pz;
        }
    }

    return best;
}

//  Criacao e remocao de vidros
void AddGlass(int type, float x, float z) {
    GlassObject g;
    InitGlass(g, (GlassType)type, x, 0.0f, z);

    // O centro fica na altura do raio da esfera, de modo que a peca sempre
    // apareca apoiada sobre o tampo da mesa.
    g.y = TABLE_Y + g.thickness * 0.5f + 0.02f;

    gGlasses.push_back(g);
    gSelectedGlass = (int)gGlasses.size() - 1;
    gSelectedLight = -1;
    gTraceDirty = true;
}

void RemoveGlass(int index) {
    if (index < 0 || index >= (int)gGlasses.size()) return;
    gGlasses.erase(gGlasses.begin() + index);
    if (gSelectedGlass >= (int)gGlasses.size()) gSelectedGlass = (int)gGlasses.size() - 1;
    gTraceDirty = true;
}

void ClearGlasses() {
    gGlasses.clear();
    gSelectedGlass = -1;
    gTraceDirty = true;
}

void ResetSimulation() {
    gGlasses.clear();
    gLights.clear();

    // Uma lente no centro e uma janela inclinada a frente.
    GlassObject lens;
    InitGlass(lens, GT_LENS, 0.30f, 0.0f, 0.0f);
    lens.y = TABLE_Y + lens.thickness * 0.5f + 0.02f;
    gGlasses.push_back(lens);

    GlassObject window;
    InitGlass(window, GT_WINDOW, -1.40f, 0.0f, 0.10f);
    window.yaw = 22.0f;
    window.y = TABLE_Y + window.thickness * 0.5f + 0.02f;
    gGlasses.push_back(window);

    // Oito fontes de luz, uma para cada cor do espectro visivel: mostra que
    // cada comprimento de onda sofre um desvio diferente ao atravessar o vidro.
    for (int i = 0; i < 8; i++) {
        LightSource l;
        float angle = 2.0f * (float)M_PI * (float)i / 8.0f;
        l.x = 2.60f * cosf(angle);
        l.y = 1.05f;
        l.z = 2.60f * sinf(angle);
        l.diverging = true;
        gLights.push_back(l);
    }

    gSelectedGlass = -1;
    gSelectedLight = 0;
    gTraceDirty = true;
}

//  Movimento
void MoveSelectedGlass(float dx, float dy) {
    if (gSelectedGlass < 0 || gSelectedGlass >= (int)gGlasses.size()) return;

    GlassObject& g = gGlasses[gSelectedGlass];

    Vec3 axis = GlassAxis(g);
    Vec3 right = Normalize(Cross(MakeVec(0.0f, 1.0f, 0.0f), axis));
    Vec3 up = MakeVec(0.0f, 1.0f, 0.0f);

    Vec3 delta = Add(Scale(right, dx), Scale(up, dy));
    g.x += delta.x;
    g.y += delta.y;
    g.z += delta.z;

    gTraceDirty = true;
}

void DragSelectedGlass(float targetX, float targetZ) {
    if (gSelectedGlass < 0 || gSelectedGlass >= (int)gGlasses.size()) return;

    GlassObject& g = gGlasses[gSelectedGlass];
    g.x = Clampf(targetX, -2.60f, 2.60f);
    g.z = Clampf(targetZ, -2.60f, 2.60f);
    gTraceDirty = true;
}

void MoveSelectedLight(float dx, float dy) {
    if (gSelectedLight < 0 || gSelectedLight >= (int)gLights.size()) return;

    LightSource& l = gLights[gSelectedLight];
    l.x = Clampf(l.x + dx, -3.0f, 3.0f);
    l.y = Clampf(l.y + dy, 0.2f, 3.0f);

    gTraceDirty = true;
}

//  Tracado de raios
void BuildRayPaths(std::vector<RayPath>& out) {
    out.clear();

    Vec3 target = MakeVec(0.0f, TABLE_Y + kSphereRadius, 0.0f);
    if (!gGlasses.empty()) {
        int index = (gSelectedGlass >= 0 && gSelectedGlass < (int)gGlasses.size()) ? gSelectedGlass : 0;
        target = MakeVec(gGlasses[index].x, gGlasses[index].y, gGlasses[index].z);
    }

    for (size_t l = 0; l < gLights.size(); l++) {
        Vec3 origin = MakeVec(gLights[l].x, gLights[l].y, gLights[l].z);
        Vec3 toTarget = Sub(target, origin);
        float distance = Length(toTarget);
        Vec3 axisDir = (distance > 1e-4f) ? Scale(toTarget, 1.0f / distance) : MakeVec(1.0f, 0.0f, 0.0f);

        Vec3 right = Normalize(Cross(MakeVec(0.0f, 1.0f, 0.0f), axisDir));
        Vec3 up = Normalize(Cross(axisDir, right));

        float spread = gLights[l].diverging ? 0.30f : 0.10f;

        for (int iy = -2; iy <= 2; iy++) {
            for (int ix = -2; ix <= 2; ix++) {
                Vec3 aim = Add(target,
                               Add(Scale(right, ix * spread * distance * 0.30f),
                                   Scale(up, iy * spread * distance * 0.30f)));

                Vec3 dir = Normalize(Sub(aim, origin));

                GlassParams traceParams;
                traceParams.glasses = gGlasses;
                std::vector<Vec3> path = TraceScene(traceParams, origin, dir, 26.0f);
                if (path.size() < 2) continue;

                RayPath rp;
                rp.color[0] = 1.0f;
                rp.color[1] = 1.0f;
                rp.color[2] = 1.0f;

                switch (l) {
                    case 0:
                        rp.color[0] = 1.00f;
                        rp.color[1] = 0.20f;
                        rp.color[2] = 0.15f;
                        break;
                    case 1:
                        rp.color[0] = 1.00f;
                        rp.color[1] = 0.45f;
                        rp.color[2] = 0.10f;
                        break;
                    case 2:
                        rp.color[0] = 1.00f;
                        rp.color[1] = 0.85f;
                        rp.color[2] = 0.10f;
                        break;
                    case 3:
                        rp.color[0] = 0.25f;
                        rp.color[1] = 0.95f;
                        rp.color[2] = 0.25f;
                        break;
                    case 4:
                        rp.color[0] = 0.10f;
                        rp.color[1] = 0.80f;
                        rp.color[2] = 1.00f;
                        break;
                    case 5:
                        rp.color[0] = 0.20f;
                        rp.color[1] = 0.30f;
                        rp.color[2] = 1.00f;
                        break;
                    case 6:
                        rp.color[0] = 0.62f;
                        rp.color[1] = 0.18f;
                        rp.color[2] = 0.90f;
                        break;
                    default:
                        rp.color[0] = 0.90f;
                        rp.color[1] = 0.90f;
                        rp.color[2] = 0.90f;
                        break;
                }

                rp.points = path;
                out.push_back(rp);
            }
        }
    }
}

//  Desenho da cena: grade, mesas e vidros
static void DrawTableGrid() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    glColor4f(0.24f, 0.30f, 0.44f, 0.55f);
    glBegin(GL_LINES);
    for (int i = -3; i <= 3; i++) {
        glVertex3f((float)i, TABLE_Y + 0.004f, -TABLE_HALF);
        glVertex3f((float)i, TABLE_Y + 0.004f, TABLE_HALF);
        glVertex3f(-TABLE_HALF, TABLE_Y + 0.004f, (float)i);
        glVertex3f(TABLE_HALF, TABLE_Y + 0.004f, (float)i);
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopAttrib();
}

// Janela e vitrine sao placas retangulares de vidro fino; as demais pecas
// ocupam o volume da esfera de raio thickness/2 e por isso refratam mais a luz.
static void DrawGlassBody(const GlassObject& g) {
    float radius = max(g.thickness * 0.5f, 0.02f);

    GLfloat glassDiffuse[] = {0.22f, 0.42f, 0.55f, 0.30f};
    GLfloat glassAmbient[] = {0.08f, 0.15f, 0.20f, 0.30f};
    GLfloat glassSpecular[] = {0.95f, 0.98f, 1.00f, 1.00f};

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, glassDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, glassAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, glassSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 120.0f);

    if (g.type == GT_WINDOW || g.type == GT_SHOWCASE) {
        float ax = radius;
        float ay = g.width * 0.5f;

        glBegin(GL_QUADS);

        // faces de entrada (-x) e saida (+x)
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-ax, -ay, -ay);
        glVertex3f(-ax, -ay, ay);
        glVertex3f(-ax, ay, ay);
        glVertex3f(-ax, ay, -ay);

        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(ax, -ay, -ay);
        glVertex3f(ax, ay, -ay);
        glVertex3f(ax, ay, ay);
        glVertex3f(ax, -ay, ay);

        // bordas
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-ax, -ay, -ay);
        glVertex3f(ax, -ay, -ay);
        glVertex3f(ax, -ay, ay);
        glVertex3f(-ax, -ay, ay);

        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-ax, ay, -ay);
        glVertex3f(-ax, ay, ay);
        glVertex3f(ax, ay, ay);
        glVertex3f(ax, ay, -ay);

        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-ax, -ay, -ay);
        glVertex3f(-ax, ay, -ay);
        glVertex3f(ax, ay, -ay);
        glVertex3f(ax, -ay, -ay);

        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-ax, -ay, ay);
        glVertex3f(ax, -ay, ay);
        glVertex3f(ax, ay, ay);
        glVertex3f(-ax, ay, ay);

        glEnd();
        return;
    }

    if (g.type == GT_PRISM) {
        // Prisma triangular: o angulo das faces separa as cores (dispersao).
        float ax = radius;
        float ay = radius;

        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-ax, -ay, 0.0f);
        glVertex3f(ax, -ay, 0.0f);
        glVertex3f(0.10f * ax, ay, 0.0f);
        glEnd();

        glBegin(GL_QUADS);
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-ax, -ay, -radius);
        glVertex3f(ax, -ay, -radius);
        glVertex3f(ax, -ay, radius);
        glVertex3f(-ax, -ay, radius);

        glNormal3f(0.91f, 0.41f, 0.0f);
        glVertex3f(-ax, -ay, -radius);
        glVertex3f(-ax, -ay, radius);
        glVertex3f(0.10f * ax, ay, radius);
        glVertex3f(0.10f * ax, ay, -radius);

        glNormal3f(-0.83f, 0.55f, 0.0f);
        glVertex3f(ax, -ay, -radius);
        glVertex3f(0.10f * ax, ay, -radius);
        glVertex3f(0.10f * ax, ay, radius);
        glVertex3f(ax, -ay, radius);
        glEnd();

        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-ax, -ay, -radius);
        glVertex3f(0.10f * ax, ay, -radius);
        glVertex3f(ax, -ay, -radius);
        glEnd();
        return;
    }

    // Lentes, lupa e garrafa: volume esferico (a mesma forma resolvida na refracao).
    DrawSphere(radius, 28, 32);

    if (g.type == GT_MAGNIFIER) {
        glDisable(GL_LIGHTING);
        glLineWidth(6.0f);
        glColor3f(0.32f, 0.25f, 0.18f);
        glBegin(GL_LINES);
        glVertex3f(radius * 1.05f, 0.0f, 0.0f);
        glVertex3f(radius * 1.05f + 1.05f, 0.0f, 0.0f);
        glEnd();
        glLineWidth(1.0f);
        glEnable(GL_LIGHTING);
    }
}

// Realce da peca selecionada: halo suave e circulo da abertura.
static void DrawSelectionHighlight(const GlassObject& g) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float radius = max(g.thickness * 0.5f, 0.02f) * 1.05f;

    glColor4f(1.0f, 0.78f, 0.25f, 0.16f);
    DrawSphere(radius + 0.05f, 20, 22);

    glColor4f(1.0f, 0.88f, 0.45f, 0.60f);
    glLineWidth(1.8f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 64; i++) {
        float a = 2.0f * (float)M_PI * i / 64.0f;
        glVertex3f(cosf(a) * g.width * 0.5f, 0.0f, sinf(a) * g.width * 0.5f);
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopAttrib();
}

//  Desenho da cena 3D
void DrawSimulationScene() {
    gTime += 0.016f;

    // Iluminacao global da bancada
    GLfloat ambient[] = {0.055f, 0.060f, 0.085f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    GLfloat lightPos[] = {3.0f, 5.0f, 4.0f, 1.0f};
    GLfloat lightDiffuse[] = {0.55f, 0.60f, 0.72f, 1.0f};
    GLfloat lightSpecular[] = {0.60f, 0.65f, 0.80f, 1.0f};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

    // Mesa
    GLfloat tableDiffuse[] = {0.13f, 0.14f, 0.19f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, tableDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, tableDiffuse);
    GLfloat tableSpec[] = {0.20f, 0.22f, 0.28f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, tableSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20.0f);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-TABLE_HALF, TABLE_Y, -TABLE_HALF);
    glVertex3f(TABLE_HALF, TABLE_Y, -TABLE_HALF);
    glVertex3f(TABLE_HALF, TABLE_Y, TABLE_HALF);
    glVertex3f(-TABLE_HALF, TABLE_Y, TABLE_HALF);
    glEnd();

    if (gShowGrid) DrawTableGrid();

    // Vidros
    if (gShowObjects) {
        for (size_t i = 0; i < gGlasses.size(); i++) {
            const GlassObject& g = gGlasses[i];

            glPushMatrix();
            glTranslatef(g.x, g.y, g.z);
            glRotatef(g.yaw, 0.0f, 1.0f, 0.0f);
            DrawGlassBody(g);
            glPopMatrix();

            if ((int)i == gSelectedGlass) DrawSelectionHighlight(g);
        }
    }

    // Raios de luz (sempre por cima, para o tracado continuar legivel)
    if (gShowRays) {
        static std::vector<RayPath> cached;
        if (gTraceDirty) {
            BuildRayPaths(cached);
            gTraceDirty = false;
        }

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glLineWidth(1.4f);

        for (size_t i = 0; i < cached.size(); i++) {
            const RayPath& rp = cached[i];
            if (rp.points.size() < 2) continue;

            // Alfa alto porque sao necessarios muitos raios finos para o feixe
            // ficar legivel; as cores identificam cada fonte de luz.
            glColor4f(rp.color[0], rp.color[1], rp.color[2], 0.85f);

            glBegin(GL_LINE_STRIP);
            for (size_t k = 0; k < rp.points.size(); k++) {
                glVertex3f(rp.points[k].x, rp.points[k].y, rp.points[k].z);
            }
            glEnd();

            const Vec3& last = rp.points[rp.points.size() - 1];
            glPointSize(4.5f);
            glBegin(GL_POINTS);
            glVertex3f(last.x, last.y, last.z);
            glEnd();
        }

        glDisable(GL_BLEND);
        glPopAttrib();
        glEnable(GL_DEPTH_TEST);
    }

    // Fontes de luz
    if (gShowLightGizmo) {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        for (size_t i = 0; i < gLights.size(); i++) {
            const LightSource& l = gLights[i];

            glPushMatrix();
            glTranslatef(l.x, l.y, l.z);

            float pulse = 1.0f + 0.07f * sinf(gTime * 3.0f + (float)i);

            if ((int)i == gSelectedLight && gSelectedLight >= 0) {
                glColor4f(1.0f, 0.85f, 0.35f, 0.28f);
                DrawSphere(0.42f * pulse, 16, 16);
                glColor4f(0.45f, 0.85f, 1.0f, 0.55f);
                glLineWidth(2.0f);
                glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 40; k++) {
                    float a = 2.0f * (float)M_PI * k / 40.0f;
                    glVertex3f(cosf(a) * 0.26f, sinf(a) * 0.26f, 0.0f);
                }
                glEnd();
                glLineWidth(1.0f);
            }

            glColor4f(l.color[0], l.color[1], l.color[2], 1.0f);
            DrawSphere(0.10f, 14, 14);

            glPopMatrix();
        }

        glDisable(GL_BLEND);
        glPopAttrib();
    }
}

//  Painel de controle
static float PanelSlider(const char* label, float x, float y, float width, float value,
                         float lo, float hi, int mx, int my, bool clicked, UiState* ui, const char* fmt) {
    PrintString(x, y + 6.0f, label, fontBaseRegular, 0.62f, 0.68f, 0.78f);

    float trackY = y - 8.0f;
    DrawRect(x, trackY - 3.0f, width, 6.0f, 0.17f, 0.19f, 0.27f, 1.0f);

    float t = Clampf((value - lo) / (hi - lo), 0.0f, 1.0f);
    DrawRect(x, trackY - 3.0f, width * t, 6.0f, 0.24f, 0.58f, 0.95f, 1.0f);
    DrawRect(x + width * t - 3.5f, trackY - 8.0f, 7.0f, 16.0f, 0.86f, 0.93f, 1.00f, 1.0f);
    DrawRectOutline(x, trackY - 3.0f, width, 6.0f, 0.32f, 0.42f, 0.60f, 1.0f);

    char buf[64];
    snprintf(buf, sizeof(buf), fmt, value);
    PrintString(x + width - strlen(buf) * 7.0f, y + 6.0f, buf, fontBaseBold, 0.86f, 0.92f, 1.00f);

    bool hoverTrack = (mx >= x - 4.0f && mx <= x + width + 4.0f && my >= trackY - 10.0f && my <= trackY + 10.0f);

    if (ui->sliderActive == 0 && hoverTrack && clicked) {
        ui->sliderActive = ui->nextSliderId;
    }

    if (ui->sliderActive == ui->nextSliderId && ui->mouseDown) {
        float nt = Clampf((mx - x) / width, 0.0f, 1.0f);
        value = lo + nt * (hi - lo);
    } else if (ui->sliderActive == ui->nextSliderId) {
        ui->sliderActive = 0;
    }

    ui->nextSliderId++;
    return value;
}

void DrawSimTabPanel(int mx, int my, bool clicked, bool mouseDown, UiState* ui) {
    ui->nextSliderId = 1;

    float x = PANEL_X + 22.0f;
    float width = PANEL_W - 44.0f;
    float y = 250.0f;

    PrintString(PANEL_X + 22.0f, 766.0f, "BANCADA OPTICA 3D", fontBaseLarge, 0.86f, 0.92f, 1.00f);
    PrintString(PANEL_X + 22.0f, 744.0f, "Refracao, reflexao total e dispersao da luz no vidro", fontBaseRegular, 0.55f, 0.60f, 0.70f);

    bool hasSelection = (gSelectedGlass >= 0 && gSelectedGlass < (int)gGlasses.size());
    char buf[180];

    // peca selecionada
    PrintString(x, y + 12.0f, "PECA SELECIONADA", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 14.0f;

    if (hasSelection) {
        const GlassObject& g = gGlasses[gSelectedGlass];
        snprintf(buf, sizeof(buf), "%s   (#%d de %d)", GlassTypeName(g.type), gSelectedGlass + 1, (int)gGlasses.size());
        PrintString(x, y, buf, fontBaseBold, 0.98f, 0.84f, 0.45f);
        y -= 16.0f;
        PrintString(x, y, "Arraste a peca na cena para reposiciona-la.", fontBaseRegular, 0.55f, 0.60f, 0.70f);
    } else {
        PrintString(x, y, "Nenhuma peca selecionada", fontBaseBold, 0.72f, 0.74f, 0.84f);
        y -= 16.0f;
        PrintString(x, y, "Clique em um vidro da biblioteca para adicionar.", fontBaseRegular, 0.55f, 0.60f, 0.70f);
    }

    y -= 26.0f;

    // biblioteca de vidros
    PrintString(x, y, "ADICIONAR VIDRO", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 10.0f;

    float cardW = 120.0f;
    float cardH = 94.0f;
    float gap = 9.0f;

    for (int t = 0; t < GT_TYPE_COUNT; t++) {
        int col = t % 3;
        int row = t / 3;

        float bx = x + col * (cardW + gap);
        float by = y - row * (cardH + gap) - cardH;

        bool hovered = (mx >= bx && mx <= bx + cardW && my >= by && my <= by + cardH);
        float r = 0.12f, g = 0.14f, b = 0.21f;
        if (hovered) {
            r = 0.17f;
            g = 0.23f;
            b = 0.35f;
        }

        DrawRect(bx, by, cardW, cardH, r, g, b, 0.95f);
        DrawRectOutline(bx, by, cardW, cardH, 0.30f, 0.42f, 0.62f, 1.5f);

        DrawGlassIcon((GlassType)t, bx + cardW * 0.5f, by + cardH - 38.0f, 1.0f);

        const char* name = GlassTypeName((GlassType)t);
        float tx = bx + (cardW - strlen(name) * 7.0f) * 0.5f;
        PrintString(tx, by + 8.0f, name, fontBaseBold, 0.90f, 0.93f, 1.00f);

        bool complete = hovered && clicked;
        bool released = hovered && !mouseDown && ui->placement == PM_DRAG && ui->dragType == t && ui->draggingIcon;

        if (complete && ui->placement == PM_DRAG) {
            ui->dragType = t;
            ui->draggingIcon = true;
        }

        if (released) {
            float placeX = 0.0f, placeZ = 0.0f;
            if (ui->dropValid && ScreenToTable(mx, my, &placeX, &placeZ)) {
                AddGlass(t, placeX, placeZ);
            } else {
                placeX = -0.9f + 0.9f * (float)(gGlasses.size() % 4);
                placeZ = -0.9f + 0.9f * (float)((gGlasses.size() / 4) % 4);
                AddGlass(t, placeX, placeZ);
            }
            ui->draggingIcon = false;
            ui->dirty = true;
        }

        if (ui->placement == PM_CLICK && complete) {
            float placeX = -0.9f + 0.9f * (float)(gGlasses.size() % 4);
            float placeZ = -0.9f + 0.9f * (float)((gGlasses.size() / 4) % 4);
            if (gGlasses.empty()) { placeX = 0.0f; placeZ = 0.0f; }
            AddGlass(t, placeX, placeZ);
            ui->draggingIcon = false;
            ui->dirty = true;
        }
    }

    y -= 2.0f * (cardH + gap) + 14.0f;

    // botoes da bancada
    float halfW = (width - 10.0f) * 0.5f;

    if (DrawButton(x, y, halfW, 28.0f, "Remover peca", false, mx, my, clicked) && hasSelection) {
        RemoveGlass(gSelectedGlass);
        ui->dirty = true;
    }
    if (DrawButton(x + halfW + 10.0f, y, halfW, 28.0f, "Limpar mesa", false, mx, my, clicked)) {
        ClearGlasses();
        ui->dirty = true;
    }
    y -= 34.0f;

    if (DrawButton(x, y, width, 28.0f, "Restaurar montagem inicial", false, mx, my, clicked)) {
        ResetSimulation();
        ui->dirty = true;
    }
    y -= 42.0f;

    // fonte de luz
    PrintString(x, y, "FONTE DE LUZ", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 22.0f;

    bool hasLight = (gSelectedLight >= 0 && gSelectedLight < (int)gLights.size());
    float thirdW = (width - 2.0f * 8.0f) / 3.0f;

    if (hasLight) {
        snprintf(buf, sizeof(buf), "Fonte #%d de %d selecionada", gSelectedLight + 1, (int)gLights.size());
        PrintString(x, y, buf, fontBaseRegular, 0.60f, 0.66f, 0.76f);
    } else {
        PrintString(x, y, "Todas as fontes fixas na cena", fontBaseRegular, 0.60f, 0.66f, 0.76f);
    }
    y -= 26.0f;

    if (DrawButton(x, y, thirdW, 26.0f, "< anterior", false, mx, my, clicked) && !gLights.empty()) {
        if (gSelectedLight < 0) gSelectedLight = 0;
        else gSelectedLight = (gSelectedLight - 1 + (int)gLights.size()) % (int)gLights.size();
        ui->dirty = true;
    }
    if (DrawButton(x + thirdW + 8.0f, y, thirdW, 26.0f, "proxima >", false, mx, my, clicked) && !gLights.empty()) {
        if (gSelectedLight < 0) gSelectedLight = 0;
        else gSelectedLight = (gSelectedLight + 1) % (int)gLights.size();
        ui->dirty = true;
    }
    if (DrawButton(x + 2.0f * (thirdW + 8.0f), y, thirdW, 26.0f, "nenhuma", false, mx, my, clicked)) {
        gSelectedLight = -1;
        ui->dirty = true;
    }
    y -= 36.0f;

    if (hasLight) {
        LightSource& l = gLights[gSelectedLight];

        float newY = PanelSlider("Altura da fonte (Y) : tecla W / S", x, y, width, l.y, 0.2f, 3.0f, mx, my, clicked, ui, "%.2f");
        if (newY != l.y) {
            l.y = newY;
            ui->dirty = true;
        }
        y -= 38.0f;

        float newX = PanelSlider("Posicao da fonte (X) : tecla A / D", x, y, width, l.x, -3.0f, 3.0f, mx, my, clicked, ui, "%.2f");
        if (newX != l.x) {
            l.x = newX;
            ui->dirty = true;
        }
        y -= 38.0f;

        float newZ = PanelSlider("Profundidade da fonte (Z) : teclas seta", x, y, width, l.z, -3.0f, 3.0f, mx, my, clicked, ui, "%.2f");
        if (newZ != l.z) {
            l.z = newZ;
            ui->dirty = true;
        }
        y -= 38.0f;

        if (DrawButton(x, y, width, 28.0f, l.diverging ? "Feixe: aberto (divergente)" : "Feixe: estreito (colimado)",
                       l.diverging, mx, my, clicked)) {
            l.diverging = !l.diverging;
            ui->dirty = true;
        }
        y -= 40.0f;
    }

    // visualizacao
    PrintString(x, y, "VISUALIZACAO", fontBaseBold, 0.62f, 0.68f, 0.80f);
    y -= 34.0f;

    if (DrawButton(x, y, halfW, 28.0f, gShowRays ? "Raios: ON" : "Raios: OFF", gShowRays, mx, my, clicked)) {
        gShowRays = !gShowRays;
    }
    if (DrawButton(x + halfW + 10.0f, y, halfW, 28.0f, gShowObjects ? "Vidros: ON" : "Vidros: OFF", gShowObjects, mx, my, clicked)) {
        gShowObjects = !gShowObjects;
    }
    y -= 34.0f;

    if (DrawButton(x, y, halfW, 28.0f, gShowLightGizmo ? "Fontes: ON" : "Fontes: OFF", gShowLightGizmo, mx, my, clicked)) {
        gShowLightGizmo = !gShowLightGizmo;
    }
    if (DrawButton(x + halfW + 10.0f, y, halfW, 28.0f, gShowGrid ? "Grade: ON" : "Grade: OFF", gShowGrid, mx, my, clicked)) {
        gShowGrid = !gShowGrid;
    }
    y -= 34.0f;

    if (DrawButton(x, y, width, 28.0f, gAutoRotate ? "Girar camera: ON" : "Girar camera: OFF", gAutoRotate, mx, my, clicked)) {
        gAutoRotate = !gAutoRotate;
    }
}

//  Entrada do mouse na cena
void SimulationTabInput(int mx, int my, bool clicked, bool mouseDown, UiState* ui, float dt) {
    (void)dt;

    if (ui->panelHover) {
        ui->dragging = false;
        return;
    }

    // No modo arrastar, a posicao de soltura e atualizada a cada quadro.
    if (ui->placement == PM_DRAG) {
        float targetX = 0.0f, targetZ = 0.0f;
        if (ScreenToTable(mx, my, &targetX, &targetZ)) {
            ui->dropX = targetX;
            ui->dropZ = targetZ;
            ui->dropValid = true;
        } else {
            ui->dropValid = false;
        }
    }

    if (clicked && !ui->draggingIcon) {
        float px = 0.0f, pz = 0.0f;
        int picked = PickGlassAt(mx, my, &px, &pz);

        if (picked >= 0) {
            gSelectedGlass = picked;
            gSelectedLight = -1;
            ui->dragging = true;
            ui->dirty = true;
        } else {
            gSelectedGlass = -1;
            ui->dirty = true;
        }
    }

    if (ui->dragging && mouseDown && gSelectedGlass >= 0) {
        float targetX = 0.0f, targetZ = 0.0f;
        if (ScreenToTable(mx, my, &targetX, &targetZ)) {
            DragSelectedGlass(targetX, targetZ);
        }
        ui->dirty = true;
    }
}

//  Teclado
void SimulationTabHandleKey(int vk, float dt, bool pressedNow, bool held) {
    bool hasSelection = (gSelectedGlass >= 0 && gSelectedGlass < (int)gGlasses.size());

    if (hasSelection && held) {
        GlassObject& g = gGlasses[gSelectedGlass];
        float step = 0.90f * dt;

        switch (vk) {
            case VK_LEFT:
                g.x -= step; gTraceDirty = true;
                return;
            case VK_RIGHT:
                g.x += step; gTraceDirty = true;
                return;
            case VK_UP:
                g.z -= step; gTraceDirty = true;
                return;
            case VK_DOWN:
                g.z += step; gTraceDirty = true;
                return;
            case 'W':
                g.y += step; gTraceDirty = true;
                return;
            case 'S':
                g.y -= step; gTraceDirty = true;
                return;
            case 'A':
                g.yaw -= 45.0f * dt; gTraceDirty = true;
                return;
            case 'D':
                g.yaw += 45.0f * dt; gTraceDirty = true;
                return;
            case 'Q':
                g.ior = Clampf(g.ior - 0.35f * dt, 1.01f, 2.60f);
                gTraceDirty = true;
                return;
            case 'E':
                g.ior = Clampf(g.ior + 0.35f * dt, 1.01f, 2.60f);
                gTraceDirty = true;
                return;
            default: break;
        }
    }

    bool hasLight = (gSelectedLight >= 0 && gSelectedLight < (int)gLights.size());

    if (hasLight && held) {
        LightSource& l = gLights[gSelectedLight];
        float step = 1.60f * dt;
        float update = true;

        switch (vk) {
            case VK_LEFT:
                l.x -= step;
                break;
            case VK_RIGHT:
                l.x += step;
                break;
            case VK_UP:
                l.z -= step;
                break;
            case VK_DOWN:
                l.z += step;
                break;
            case 'W':
                l.y += step;
                break;
            case 'S':
                l.y -= step;
                break;
            default:
                update = false;
                break;
        }

        if (update) {
            l.x = Clampf(l.x, -3.0f, 3.0f);
            l.y = Clampf(l.y, 0.2f, 3.0f);
            l.z = Clampf(l.z, -3.0f, 3.0f);
            gTraceDirty = true;
            return;
        }
    }

    if (!pressedNow) return;

    switch (vk) {
        case 'R':
            ResetSimulation();
            return;
        case 'G':
            gShowGrid = !gShowGrid;
            return;
        case 'T':
            gShowRays = !gShowRays;
            return;
        case 'C':
            gShowObjects = !gShowObjects;
            return;
        case 'L':
            gShowLightGizmo = !gShowLightGizmo;
            return;
        case VK_DELETE:
            if (hasSelection) RemoveGlass(gSelectedGlass);
            return;
        case '1': case '2': case '3': case '4': case '5': case '6':
            {
                // Digitos 1..6 adicionam o vidro correspondente no centro da mesa.
                float placeX = gGlasses.empty() ? 0.0f : -0.9f + 0.9f * (float)(gGlasses.size() % 4);
                float placeZ = gGlasses.empty() ? 0.0f : -0.9f + 0.9f * (float)((gGlasses.size() / 4) % 4);
                AddGlass(vk - '1', placeX, placeZ);
            }
            return;
        default:
            break;
    }
}

//  API publica
void SimulationTabDraw() {
    // O tracado dos raios e reconstruido sob demanda em DrawSimulationScene().
}

void SimulationTabSetCameraRef(Camera* cam) {
    gCamera = cam;
}

void SimulationTabSetHelpVisible(bool visible) {
    gHelpVisible = visible;
}

bool SimulationTabHelpVisible() {
    return gHelpVisible;
}
