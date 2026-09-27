#include "optics.h"
#include "window.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <windows.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

//  Constantes locais do tracado
static const float RAY_LENGTH = 26.0f; // comprimento maximo de um raio
static const float RAY_EPS = 1e-3f; // tolerancia para nao reacertar a propria superficie
// TABLE_Y e TABLE_HALF sao definidas em optics.h.

//  Catalogo de materiais (vidros reais e suas dispersoes)
struct MaterialEntry {
    const char* name;
    float ior[3]; // indice de refracao em R, G e B
};

static const MaterialEntry MATERIALS[] = {
    {"Vidro Crown (comum)", {1.514f, 1.520f, 1.528f}},
    {"Vidro Flint (denso)", {1.612f, 1.625f, 1.642f}},
    {"Acrilico (PMMA)", {1.485f, 1.490f, 1.497f}},
    {"Quartzo Fundido", {1.454f, 1.458f, 1.463f}},
    {"Diamante", {2.407f, 2.417f, 2.435f}}
};

static const int MATERIAL_COUNT = (int)(sizeof(MATERIALS) / sizeof(MATERIALS[0]));

int MaterialCatalogSize() {
    return MATERIAL_COUNT;
}

const char* MaterialName(int index) {
    if (index < 0 || index >= MATERIAL_COUNT) index = 0;
    return MATERIALS[index].name;
}

float MaterialIor(int index, int channel) {
    if (index < 0 || index >= MATERIAL_COUNT) index = 0;
    if (channel < 0 || channel > 2) channel = 1;
    return MATERIALS[index].ior[channel];
}

//  Vetores
static Vec3 MakeVec(float x, float y, float z) {
    Vec3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

static Vec3 Add(const Vec3& a, const Vec3& b) {
    return MakeVec(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 Sub(const Vec3& a, const Vec3& b) {
    return MakeVec(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 Scale(const Vec3& a, float s) {
    return MakeVec(a.x * s, a.y * s, a.z * s);
}

static float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 Cross(const Vec3& a, const Vec3& b) {
    return MakeVec(a.y * b.z - a.z * b.y,
                   a.z * b.x - a.x * b.z,
                   a.x * b.y - a.y * b.x);
}

static float Length(const Vec3& a) {
    return sqrtf(Dot(a, a));
}

static Vec3 Normalize(const Vec3& a) {
    float len = Length(a);
    if (len < 1e-6f) return MakeVec(0.0f, 0.0f, 1.0f);
    return Scale(a, 1.0f / len);
}

static float Clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

//  Tipos de vidro
const char* GlassTypeName(GlassType type) {
    switch (type) {
        case GT_WINDOW: return "Janela";
        case GT_SHOWCASE: return "Vitrine";
        case GT_LENS: return "Lente";
        case GT_MAGNIFIER: return "Lupa";
        case GT_BOTTLE: return "Garrafa";
        case GT_PRISM: return "Prisma";
        default: return "Vidro";
    }
}

void InitGlass(GlassObject& g, GlassType type, float x, float y, float z) {
    g = GlassObject();
    g.type = type;
    g.x = x;
    g.y = y;
    g.z = z;
    g.yaw = 0.0f;

    switch (type) {
        case GT_WINDOW:
            g.width = 1.90f;
            g.thickness = 0.10f;
            g.ior = 1.52f;
            g.square = true;
            break;
        case GT_SHOWCASE:
            g.width = 1.60f;
            g.thickness = 0.48f;
            g.ior = 1.52f;
            g.square = true;
            break;
        case GT_LENS:
            g.width = 1.30f;
            g.thickness = 0.55f;
            g.ior = 1.52f;
            g.square = false;
            break;
        case GT_MAGNIFIER:
            g.width = 1.80f;
            g.thickness = 0.90f;
            g.ior = 1.52f;
            g.square = false;
            break;
        case GT_BOTTLE:
            g.width = 1.05f;
            g.thickness = 1.05f;
            g.ior = 1.52f;
            g.square = false;
            break;
        case GT_PRISM:
            g.width = 1.15f;
            g.thickness = 1.35f;
            g.ior = 1.52f;
            g.square = true;
            break;
        default:
            g.width = 1.30f;
            g.thickness = 0.50f;
            g.ior = 1.52f;
            g.square = false;
            break;
    }
}

GlassParams DefaultGlassParams() {
    GlassParams params;
    params.glasses.clear();

    GlassObject lens;
    InitGlass(lens, GT_LENS, 0.10f, 0.05f, 0.0f);
    params.glasses.push_back(lens);

    GlassObject window;
    InitGlass(window, GT_WINDOW, -1.55f, 0.10f, 0.0f);
    window.yaw = 24.0f;
    params.glasses.push_back(window);

    params.light.x = -2.60f;
    params.light.y = 1.05f;
    params.light.z = 0.45f;
    params.light.diverging = false;
    params.maxReflections = 2;

    return params;
}

//  Geometria: superficie esferica e recorte do vidro
Vec3 GlassAxis(const GlassObject& g) {
    float a = g.yaw * (float)M_PI / 180.0f;
    return MakeVec(sinf(a), 0.0f, cosf(a));
}

// Ponto de entrada do raio na esfera de raio r centrada na origem.
// Devolve a menor raiz positiva de |o + t*d|^2 = r^2.
static bool RaySphere(const Vec3& o, const Vec3& d, float r, float* t) {
    float b = Dot(o, d);
    float c = Dot(o, o) - r * r;
    float disc = b * b - c;
    if (disc < 0.0f) return false;

    float sq = sqrtf(disc);
    float t0 = -b - sq;
    float t1 = -b + sq;

    if (t0 > RAY_EPS) {
        *t = t0;
        return true;
    }
    if (t1 > RAY_EPS) {
        *t = t1;
        return true;
    }
    return false;
}

// Funcao de recorte: quadrado de meia-largura R (janela) ou circulo de raio R.
static float ApertureValue(const GlassObject& g, const Vec3& local) {
    float halfWidth = g.width * 0.5f;
    if (g.square) return std::max(fabsf(local.x), fabsf(local.z)) - halfWidth;
    return sqrtf(local.x * local.x + local.z * local.z) - halfWidth;
}

// Interseccao do raio com o vidro.  O corpo optico e sempre a esfera de raio
// thickness/2 recortada pela abertura do tipo (quadrado = janela, circulo =
// lentes, lupa, garrafa e prisma).  Devolve a distancia da superficie de
// entrada e a normal orientada para o lado de onde a luz vem.
static bool IntersectGlass(const GlassObject& g, const Vec3& origin, const Vec3& dir, float* tHit, Vec3* normal) {
    if (!g.active) return false;

    float r = g.thickness * 0.5f;
    if (r < 0.02f) r = 0.02f;

    Vec3 axis = GlassAxis(g);
    Vec3 right = Normalize(Cross(MakeVec(0.0f, 1.0f, 0.0f), axis));
    Vec3 up = MakeVec(0.0f, 1.0f, 0.0f);

    Vec3 rel = Sub(origin, MakeVec(g.x, g.y, g.z));
    Vec3 o = MakeVec(Dot(rel, axis), Dot(rel, up), Dot(rel, right));
    Vec3 d = MakeVec(Dot(dir, axis), Dot(dir, up), Dot(dir, right));

    float a = Dot(d, d);
    float b = Dot(o, d);
    float c = Dot(o, o) - r * r;
    if (a < 1e-9f) return false;

    float disc = b * b - a * c;
    if (disc < 0.0f) return false;

    float sq = sqrtf(disc);
    float t0 = (-b - sq) / a;
    float t1 = (-b + sq) / a;

    float chosen = -1.0f;
    if (t0 > RAY_EPS) chosen = t0;
    else if (t1 > RAY_EPS) chosen = t1;

    if (chosen < 0.0f) return false;

    Vec3 hitLocal = Add(o, Scale(d, chosen));
    if (ApertureValue(g, hitLocal) > 0.0f) return false;

    Vec3 nLocal = Normalize(hitLocal);

    Vec3 n = Add(Scale(axis, nLocal.x),
                 Add(Scale(up, nLocal.y), Scale(right, nLocal.z)));

    if (Dot(n, dir) > 0.0f) n = Scale(n, -1.0f);

    *tHit = chosen;
    *normal = n;
    return true;
}

bool RayHitsGlass(const GlassObject& g, const Vec3& origin, const Vec3& dir, float maxDistance, float* outDistance) {
    float t = 0.0f;
    Vec3 n;
    if (!IntersectGlass(g, origin, dir, &t, &n)) return false;
    if (t > maxDistance) return false;
    if (outDistance) *outDistance = t;
    return true;
}

//  Refracao (lei de Snell vetorial)
// n1 * sin(t1) = n2 * sin(t2).  Se a raiz for negativa ocorre reflexao total
// interna e a funcao devolve false.
static bool Refract(const Vec3& dir, const Vec3& normal, float eta, Vec3* out) {
    float cosI = -Dot(normal, dir);
    float k = 1.0f - eta * eta * (1.0f - cosI * cosI);
    if (k < 0.0f) return false;

    Vec3 refracted = Add(Scale(dir, eta), Scale(normal, eta * cosI - sqrtf(k)));
    *out = Normalize(refracted);
    return true;
}

static Vec3 Reflect(const Vec3& dir, const Vec3& normal) {
    return Normalize(Sub(dir, Scale(normal, 2.0f * Dot(dir, normal))));
}

//  Traco de um raio meridional por dois vidros (numeros de serie).
//  O plano meridional e definido por V = (d.z, 0, -d.x), perpendicular a
//  direcao de propagacao.  Nesse plano reduzido o raio e descrito por (x, z):
//  x ao longo do eixo optico e z a altura transversal. A propagacao entre as
//  superficies e a refracao em cada dioptro esferico sao resolvidas exatamente,
//  o que fornece a posicao e o tamanho da imagem.
struct MeridionalState {
    float x; // coordenada ao longo do eixo optico
    float z; // altura transversal
    float u; // tangente do angulo do raio com o eixo (dz/dx)
};

static bool TraceTwoSurfaces(const GlassObject& gA, const GlassObject& gB, int mediumIndex, float height,
                             MeridionalState* out) {
    Vec3 axisA = GlassAxis(gA);
    Vec3 axisB = GlassAxis(gB);

    Vec3 V = MakeVec(axisA.z, 0.0f, -axisA.x);
    if (Length(V) < 1e-5f) return false;
    V = Normalize(V);

    Vec3 centreA = MakeVec(gA.x, gA.y, gA.z);
    Vec3 centreB = MakeVec(gB.x, gB.y, gB.z);

    float rA = std::max(gA.thickness * 0.5f, 0.02f);
    float rB = std::max(gB.thickness * 0.5f, 0.02f);

    // Numero de serie da primeira superficie: concava, portanto o vertice (origem
// do raio meridional) fica recuado de rA a frente do centro da esfera.
    Vec3 entry = Sub(centreA, Scale(axisA, rA));
    float base = Dot(entry, axisA);

    float tanB = tanf(gB.yaw * (float)M_PI / 180.0f);
    float xB = Dot(centreB, axisA) - base;

    // Direcao inicial: raio que parte do plano de entrada com altura z.
    Vec3 start = Add(entry, Scale(V, height));
    float nIn = MaterialIor(mediumIndex, 1);

    // Superficie 1: esfera centrada em (0, 0) vista pela lateral.
    float q1 = nIn * nIn * (rA * rA - height * height);
    if (q1 < 0.0f) return false;

    float x1 = sqrtf(q1); // > 0 (concava, a esfera fica a frente)
    float z1 = height;
    float m1 = height / x1; // dz/dx no ponto de incidencia
    float n1 = x1 / rA; // componente da normal no eixo
    float m2 = -n1 / sqrtf(1.0f - n1 * n1);
    float u1 = (m2 - m1) / (1.0f + m1 * m2);

    // Propagacao ate a segunda superficie.
    float x2 = xB;
    {
        float dx = x2 - x1;
        float q2 = rB * rB - (z1 + u1 * dx) * (z1 + u1 * dx);
        if (q2 < 0.0f) return false;
        x2 = xB + sqrtf(q2);
    }

    float z2 = z1 + u1 * (x2 - x1);
    float dx2 = x2 - xB;
    float m3 = dx2 / rB; // dz/dx da normal na superficie 2
    float n2 = dx2 / rB;
    float m4 = n2 / sqrtf(1.0f - n2 * n2);
    float u2 = (m4 - m3) / (1.0f + m3 * m4);

    out->x = x2;
    out->z = z2;
    out->u = u2;
    return true;
}

RayResult ComputeSystem(const GlassParams& params) {
    RayResult result;

    std::vector<const GlassObject*> chain;
    for (size_t i = 0; i < params.glasses.size() && chain.size() < 2; i++) {
        if (params.glasses[i].active) chain.push_back(&params.glasses[i]);
    }

    if (chain.empty()) return result;
    if (chain.size() == 1) chain.push_back(chain[0]);

    int mediumIndex = (params.glasses.size() > 1) ? MED_GLASS_A : MED_GLASS_A;

    // Ajuste fino da altura do raio para maximizar o quanto ele se aproxima do
    // eixo na saida: e o suficiente para localizar o foco do sistema.
    float bestHeight = 0.0f;
    float bestZ = 1e9f;
    MeridionalState best;
    bool found = false;

    for (int i = 0; i <= 60; i++) {
        float h = -0.75f + (1.5f * i / 60.0f);
        if (fabsf(h) < 1e-4f) continue;

        MeridionalState s;
        if (!TraceTwoSurfaces(*chain[0], *chain[1], mediumIndex, h, &s)) continue;

        float err = fabsf(s.z);
        if (!found || err < bestZ) {
            found = true;
            bestZ = err;
            bestHeight = h;
            best = s;
        }
    }

    if (!found) return result;

    result.valid = true;
    result.objectY = bestHeight;
    result.imageY = best.z;
    result.imageDistance = best.x;

    if (fabsf(best.u) < 1e-4f) {
        result.imageDistance = 0.0f;
        result.real = true;
    } else {
        float crossing = best.x - best.z / best.u;
        result.real = (best.u * best.z <= 0.0f);
        result.imageDistance = crossing;
    }

    return result;
}

//  Ray tracing em 3D para a visualizacao
struct HitInfo {
    int index = -1;
    float t = 0.0f;
    Vec3 normal;
};

static bool FindNearestHit(const GlassParams& params, const Vec3& origin, const Vec3& dir, HitInfo* hit) {
    bool found = false;
    float bestT = 1e9f;

    for (size_t i = 0; i < params.glasses.size(); i++) {
        float t = 0.0f;
        Vec3 n;
        if (!IntersectGlass(params.glasses[i], origin, dir, &t, &n)) continue;
        if (t < bestT) {
            bestT = t;
            hit->index = (int)i;
            hit->t = t;
            hit->normal = n;
            found = true;
        }
    }

    return found;
}

std::vector<Vec3> TraceScene(const GlassParams& params, const Vec3& origin, const Vec3& dir, float maxLength) {
    std::vector<Vec3> path;
    path.push_back(origin);

    Vec3 position = origin;
    Vec3 direction = Normalize(dir);

    int currentIndex = -1;
    bool insideGlass = false;
    float remaining = maxLength;
    int internalReflections = 0;

    for (int bounce = 0; bounce < 16; bounce++) {
        HitInfo hit;
        if (!FindNearestHit(params, position, direction, &hit)) break;

        if (hit.t > remaining) {
            path.push_back(Add(position, Scale(direction, remaining)));
            break;
        }

        Vec3 point = Add(position, Scale(direction, hit.t));
        path.push_back(point);

        remaining -= hit.t;
        if (remaining <= 0.0f) break;

        bool entering = (hit.index != currentIndex);
        float n1 = insideGlass ? params.glasses[hit.index].ior : 1.0f;
        float n2 = insideGlass ? 1.0f : params.glasses[hit.index].ior;

        if (!entering) {
            // Reacerto da mesma superficie: segue em frente para nao travar.
            position = Add(point, Scale(direction, RAY_EPS * 4.0f));
            continue;
        }

        Vec3 refracted;
        float eta = n1 / n2;

        if (Refract(direction, hit.normal, eta, &refracted)) {
            direction = refracted;
            insideGlass = !insideGlass;
            currentIndex = insideGlass ? hit.index : -1;
        } else {
            // Limite de reflexoes internas atingido: o raio se perde no vidro.
            if (internalReflections >= params.maxReflections) break;
            internalReflections++;
            direction = Reflect(direction, hit.normal);
        }

        position = Add(point, Scale(direction, RAY_EPS * 4.0f));
    }

    return path;
}

//  Primitivas de desenho 3D
static void SetMaterial(float r, float g, float b, float alpha) {
    GLfloat diffuse[] = {r, g, b, alpha};
    GLfloat ambient[] = {r * 0.35f, g * 0.35f, b * 0.35f, alpha};
    GLfloat specular[] = {0.85f, 0.88f, 0.95f, 1.0f};

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 80.0f);
}

void DrawSphere(float radius, int stacks, int slices) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = (float)M_PI * (-0.5f + (float)i / stacks);
        float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / stacks);

        float z0 = sinf(lat0);
        float zr0 = cosf(lat0);
        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * (float)M_PI * (float)j / slices;
            float x = cosf(lng);
            float y = sinf(lng);

            glNormal3f(x * zr0, y * zr0, z0);
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glNormal3f(x * zr1, y * zr1, z1);
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}

static void DrawQuad(float halfWidth, float y, float halfDepth) {
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-halfWidth, y, -halfDepth);
    glVertex3f(halfWidth, y, -halfDepth);
    glVertex3f(halfWidth, y, halfDepth);
    glVertex3f(-halfWidth, y, halfDepth);
    glEnd();
}

static void DrawPlaneGrid() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    glColor4f(0.20f, 0.24f, 0.34f, 0.55f);
    glBegin(GL_LINES);
    for (int i = -3; i <= 3; i++) {
        glVertex3f((float)i, TABLE_Y + 0.005f, -3.0f);
        glVertex3f((float)i, TABLE_Y + 0.005f, 3.0f);
        glVertex3f(-3.0f, TABLE_Y + 0.005f, (float)i);
        glVertex3f(3.0f, TABLE_Y + 0.005f, (float)i);
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopAttrib();
}

static void DrawAxisArrows() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);

    glColor3f(0.85f, 0.30f, 0.20f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, TABLE_Y + 0.01f, 0.0f);
    glVertex3f(3.6f, TABLE_Y + 0.01f, 0.0f);
    glEnd();

    glColor3f(0.30f, 0.85f, 0.35f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, TABLE_Y + 0.01f, 0.0f);
    glVertex3f(0.0f, 2.6f, 0.0f);
    glEnd();

    glColor3f(0.35f, 0.55f, 0.95f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, TABLE_Y + 0.01f, 0.0f);
    glVertex3f(0.0f, TABLE_Y + 0.01f, 3.6f);
    glEnd();

    glPopAttrib();
}

static void DrawSelectionHighlight(const GlassObject& g) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float r = g.thickness * 0.5f * 1.06f + 0.02f;

    glColor4f(1.0f, 0.78f, 0.25f, 0.22f);
    DrawSphere(r, 20, 20);

    glColor4f(1.0f, 0.88f, 0.45f, 0.55f);
    glLineWidth(1.6f);

    float halfWidth = g.width * 0.5f;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 64; i++) {
        float a = 2.0f * (float)M_PI * i / 64.0f;
        if (g.square) {
            glVertex3f(halfWidth * cosf(a), 0.0f, halfWidth * sinf(a));
        } else {
            glVertex3f(halfWidth * cosf(a), 0.0f, halfWidth * sinf(a));
        }
    }
    glEnd();

    glDisable(GL_BLEND);
    glPopAttrib();
}

// Desenha o volume de vidro correspondente ao tipo escolhido.  Todas as pecas
// compartilham a mesma optica (esfera de raio thickness/2 recortada pela
// abertura), variando apenas a forma visual.
static void DrawGlassBody(const GlassObject& g, int materialIndex) {
    float r = std::max(g.thickness * 0.5f, 0.02f);
    float halfWidth = g.width * 0.5f;

    GLfloat tint[3] = {0.55f, 0.78f, 0.90f};
    float tintScale = 1.0f - Clampf((g.ior - 1.0f) / 2.0f, 0.0f, 0.6f);
    tintScale = Clampf(tintScale, 0.4f, 1.0f);
    tint[0] *= tintScale;
    tint[1] *= tintScale;
    tint[2] = Clampf(tint[2] * (1.35f - tintScale * 0.35f), 0.0f, 1.0f);

    SetMaterial(tint[0], tint[1], tint[2], 0.88f);

    if (g.square) {
        // Corpo com arestas: janela, vitrine e prisma.
        float ax = halfWidth;
        float ay = r;
        float az = halfWidth;

        if (g.type == GT_PRISM) az = halfWidth * 1.05f;

        glBegin(GL_QUADS);
        // faces laterais
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(ax, -ay, -az); glVertex3f(ax, ay, -az);
        glVertex3f(ax, ay, az); glVertex3f(ax, -ay, az);

        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-ax, -ay, -az); glVertex3f(-ax, -ay, az);
        glVertex3f(-ax, ay, az); glVertex3f(-ax, ay, -az);

        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-ax, -ay, az); glVertex3f(ax, -ay, az);
        glVertex3f(ax, ay, az); glVertex3f(-ax, ay, az);

        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f(-ax, -ay, -az); glVertex3f(-ax, ay, -az);
        glVertex3f(ax, ay, -az); glVertex3f(ax, -ay, -az);

        // faces de entrada e saida
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-ax, ay, -az); glVertex3f(-ax, ay, az);
        glVertex3f(ax, ay, az); glVertex3f(ax, ay, -az);

        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-ax, -ay, -az); glVertex3f(ax, -ay, -az);
        glVertex3f(ax, -ay, az); glVertex3f(-ax, -ay, az);
        glEnd();
    } else if (g.type == GT_BOTTLE) {
        // Garrafa: corpo cilindrico com gargalo e tampa.
        int segments = 40;

        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; i++) {
            float a = 2.0f * (float)M_PI * i / segments;
            float cx = cosf(a) * halfWidth;
            float cz = sinf(a) * halfWidth;
            glNormal3f(cosf(a), 0.0f, sinf(a));
            glVertex3f(cx, -r * 0.55f, cz);
            glVertex3f(cx, r * 0.15f, cz);
        }
        glEnd();

        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; i++) {
            float a = 2.0f * (float)M_PI * i / segments;
            float cx = cosf(a) * halfWidth;
            float cz = sinf(a) * halfWidth;
            glNormal3f(cosf(a), 0.35f, sinf(a));
            glVertex3f(cx, r * 0.15f, cz);
            glVertex3f(cx * 0.30f, r * 1.05f, cz * 0.30f);
        }
        glEnd();

        SetMaterial(0.30f, 0.34f, 0.42f, 0.9f);
        glPushMatrix();
        glTranslatef(0.0f, r * 1.15f, 0.0f);
        glScalef(1.0f, 0.35f, 1.0f);
        DrawSphere(halfWidth * 0.32f, 10, 16);
        glPopMatrix();
    } else {
        // Lentes e lupa: calota esferica (o modelo optico e exatamente a esfera).
        DrawSphere(r, 26, 30);

        if (g.type == GT_MAGNIFIER) {
            SetMaterial(0.35f, 0.28f, 0.20f, 0.95f);
            glLineWidth(6.0f);
            float handleX = halfWidth * 1.02f;
            glBegin(GL_LINES);
            glVertex3f(handleX, 0.0f, 0.0f);
            glVertex3f(handleX + 0.85f, 0.0f, 0.0f);
            glEnd();
            glLineWidth(1.0f);
        }
    }
}

//  Cena
void InitScene() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// Raios que formam o cone de luz.  Os cruzamentos no plano z = 0 concentram-se
// em uma linha, revelando a distorcao causada por cada vidro.
static const int RAY_OFFSETS = 3;
static const float RAY_SPACING = 0.34f;

static void DrawRays(const GlassParams& params) {
    Vec3 origin = MakeVec(params.light.x, params.light.y, params.light.z);
    Vec3 target = MakeVec(0.6f, -0.35f, 0.0f);
    Vec3 baseDir = Normalize(Sub(target, origin));

    Vec3 right = Normalize(Cross(MakeVec(0.0f, 1.0f, 0.0f), baseDir));
    Vec3 up = Normalize(Cross(baseDir, right));

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.6f);

    for (int iy = -RAY_OFFSETS; iy <= RAY_OFFSETS; iy++) {
        for (int ix = -RAY_OFFSETS; ix <= RAY_OFFSETS; ix++) {
            if (ix == 0 && iy == 0) continue;

            Vec3 aim = Add(MakeVec(0.6f, -0.35f, 0.0f),
                           Add(Scale(right, ix * RAY_SPACING * 1.6f),
                               Scale(up, iy * RAY_SPACING * 1.6f)));

            Vec3 dir = Normalize(Sub(aim, origin));
            std::vector<Vec3> path = TraceScene(params, origin, dir, RAY_LENGTH);

            glColor4f(1.0f, 0.86f, 0.42f, 0.9f);

            glBegin(GL_LINE_STRIP);
            for (size_t k = 0; k < path.size(); k++) {
                glVertex3f(path[k].x, path[k].y, path[k].z);
            }
            glEnd();

            if (path.size() >= 2) {
                const Vec3& end = path[path.size() - 1];
                glPointSize(4.0f);
                glColor4f(1.0f, 0.72f, 0.25f, 0.85f);
                glBegin(GL_POINTS);
                glVertex3f(end.x, end.y, end.z);
                glEnd();
            }
        }
    }

    glDisable(GL_BLEND);
    glPopAttrib();
}

static void DrawLightSource(const LightSource& light, float time) {
    glPushMatrix();
    glTranslatef(light.x, light.y, light.z);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    float pulse = 1.0f + 0.06f * sinf(time * 3.0f);

    glColor4f(1.0f, 0.85f, 0.35f, 0.20f);
    DrawSphere(0.46f * pulse, 18, 18);

    glColor4f(1.0f, 0.93f, 0.62f, 1.0f);
    DrawSphere(0.15f, 18, 18);

    glDisable(GL_BLEND);
    glPopAttrib();
    glPopMatrix();
}

static void SetupLights(const LightSource& light) {
    GLfloat ambient[] = {0.10f, 0.11f, 0.15f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    GLfloat pos[] = {light.x, light.y, light.z, 1.0f};
    GLfloat diffuse[] = {1.0f, 0.90f, 0.68f, 1.0f};
    GLfloat specular[] = {1.0f, 0.96f, 0.82f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

    GLfloat fillPos[] = {-2.0f, 3.2f, -2.6f, 1.0f};
    GLfloat fillDiffuse[] = {0.22f, 0.26f, 0.38f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fillDiffuse);
}

void DrawGlassScene(const GlassParams& params, float time, int selectedIndex, bool dragging) {
    SetupLights(params.light);

    // Mesa da bancada optica
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    SetMaterial(0.09f, 0.10f, 0.15f, 1.0f);
    DrawQuad(TABLE_HALF, TABLE_Y, TABLE_HALF);
    glPopAttrib();

    DrawPlaneGrid();
    DrawAxisArrows();

    // Vidros
    for (size_t i = 0; i < params.glasses.size(); i++) {
        const GlassObject& g = params.glasses[i];
        if (!g.active) continue;

        Vec3 axis = GlassAxis(g);
        float angle = atan2f(axis.x, axis.z) * 180.0f / (float)M_PI;

        glPushMatrix();
        glTranslatef(g.x, g.y, g.z);
        glRotatef(angle, 0.0f, 1.0f, 0.0f);

        DrawGlassBody(g, 0);

        if ((int)i == selectedIndex) DrawSelectionHighlight(g);

        glPopMatrix();
    }

    // Raios por cima dos vidros para que o tracado permaneca legivel
    glDisable(GL_DEPTH_TEST);
    DrawRays(params);
    glEnable(GL_DEPTH_TEST);

    DrawLightSource(params.light, time);
}

//  Icones de modelo para o painel do HUD
void DrawModelIcons() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1280, 0, 800, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for (int t = 0; t < GT_TYPE_COUNT; t++) {
        float x = 196.0f + t * 150.0f;
        float y = 206.0f;

        glColor4f(0.10f, 0.13f, 0.20f, 0.90f);
        glBegin(GL_QUADS);
        glVertex2f(x - 30.0f, y - 24.0f);
        glVertex2f(x + 30.0f, y - 24.0f);
        glVertex2f(x + 30.0f, y + 24.0f);
        glVertex2f(x - 30.0f, y + 24.0f);
        glEnd();

        glColor3f(0.32f, 0.60f, 0.88f);
        glLineWidth(1.2f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - 30.0f, y - 24.0f);
        glVertex2f(x + 30.0f, y - 24.0f);
        glVertex2f(x + 30.0f, y + 24.0f);
        glVertex2f(x - 30.0f, y + 24.0f);
        glEnd();

        glColor4f(0.55f, 0.80f, 0.95f, 0.85f);
        glLineWidth(1.8f);

        switch (t) {
            case GT_WINDOW:
                glBegin(GL_LINE_LOOP);
                glVertex2f(x - 20.0f, y - 18.0f);
                glVertex2f(x + 20.0f, y - 18.0f);
                glVertex2f(x + 20.0f, y + 18.0f);
                glVertex2f(x - 20.0f, y + 18.0f);
                glEnd();
                break;
            case GT_SHOWCASE:
                glBegin(GL_LINE_LOOP);
                glVertex2f(x - 22.0f, y - 12.0f);
                glVertex2f(x + 22.0f, y - 12.0f);
                glVertex2f(x + 22.0f, y + 12.0f);
                glVertex2f(x - 22.0f, y + 12.0f);
                glEnd();
                glBegin(GL_LINE_LOOP);
                glVertex2f(x - 16.0f, y - 12.0f);
                glVertex2f(x + 16.0f, y - 12.0f);
                glVertex2f(x + 16.0f, y + 12.0f);
                glVertex2f(x - 16.0f, y + 12.0f);
                glEnd();
                break;
            case GT_LENS:
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; i++) {
                    float a = 2.0f * (float)M_PI * i / 32.0f;
                    glVertex2f(x + cosf(a) * 16.0f, y + sinf(a) * 20.0f);
                }
                glEnd();
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; i++) {
                    float a = 2.0f * (float)M_PI * i / 32.0f;
                    glVertex2f(x + cosf(a) * 22.0f, y + sinf(a) * 20.0f);
                }
                glEnd();
                break;
            case GT_MAGNIFIER:
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; i++) {
                    float a = 2.0f * (float)M_PI * i / 32.0f;
                    glVertex2f(x - 4.0f + cosf(a) * 13.0f, y + sinf(a) * 13.0f);
                }
                glEnd();
                glBegin(GL_LINES);
                glVertex2f(x + 6.0f, y - 9.0f);
                glVertex2f(x + 20.0f, y - 21.0f);
                glEnd();
                break;
            case GT_BOTTLE:
                glBegin(GL_LINE_LOOP);
                glVertex2f(x - 11.0f, y - 20.0f);
                glVertex2f(x + 11.0f, y - 20.0f);
                glVertex2f(x + 11.0f, y + 2.0f);
                glVertex2f(x + 4.0f, y + 12.0f);
                glVertex2f(x + 4.0f, y + 20.0f);
                glVertex2f(x - 4.0f, y + 20.0f);
                glVertex2f(x - 4.0f, y + 12.0f);
                glVertex2f(x - 11.0f, y + 2.0f);
                glEnd();
                break;
            case GT_PRISM:
                glBegin(GL_LINE_LOOP);
                glVertex2f(x - 20.0f, y - 18.0f);
                glVertex2f(x + 20.0f, y - 18.0f);
                glVertex2f(x + 6.0f, y + 18.0f);
                glEnd();
                break;
            default:
                break;
        }
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
}

// ---------------------------------------------------------------------------
//  Escolha do ponto da mesa a partir de um clique
// ---------------------------------------------------------------------------
// O ponto e obtido pela interseccao do raio do pixel (unproject) com o plano do
// tampo da mesa.  A mesma conversao e usada tanto para soltar uma peca nova
// quanto para arrastar um vidro ja existente.
bool ScreenToTable(int mx, int my, float* outX, float* outZ) {
    GLdouble model[16];
    GLdouble proj[16];
    GLint viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLdouble ox = 0.0, oy = 0.0, oz = 0.0;
    GLdouble fx = 0.0, fy = 0.0, fz = 0.0;

    if (!gluUnProject((GLdouble)mx, (GLdouble)my, 0.0, model, proj, viewport, &ox, &oy, &oz)) return false;
    if (!gluUnProject((GLdouble)mx, (GLdouble)my, 1.0, model, proj, viewport, &fx, &fy, &fz)) return false;

    double dx = fx - ox;
    double dy = fy - oy;
    double dz = fz - oz;

    if (fabs(dy) < 1e-6) return false;

    double t = (TABLE_Y - oy) / dy;
    if (t < 0.0 || t > 500.0) return false;

    float px = (float)(ox + dx * t);
    float pz = (float)(oz + dz * t);

    *outX = Clampf(px, -TABLE_HALF, TABLE_HALF);
    *outZ = Clampf(pz, -TABLE_HALF, TABLE_HALF);
    return true;
}

void DrawPlacementCursor() {
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(g_hwnd, &mousePos);
    int winY = g_height - mousePos.y;

    float px = 0.0f;
    float pz = 0.0f;
    if (!ScreenToTable(mousePos.x, winY, &px, &pz)) return;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glLineWidth(1.6f);
    glColor4f(0.45f, 0.85f, 1.0f, 0.75f);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 48; i++) {
        float a = 2.0f * (float)M_PI * i / 48.0f;
        glVertex3f(px + cosf(a) * 0.28f, TABLE_Y + 0.012f, pz + sinf(a) * 0.28f);
    }
    glEnd();

    glBegin(GL_LINES);
    glVertex3f(px - 0.40f, TABLE_Y + 0.012f, pz);
    glVertex3f(px + 0.40f, TABLE_Y + 0.012f, pz);
    glVertex3f(px, TABLE_Y + 0.012f, pz - 0.40f);
    glVertex3f(px, TABLE_Y + 0.012f, pz + 0.40f);
    glEnd();

    glDisable(GL_BLEND);
    glPopAttrib();
}
