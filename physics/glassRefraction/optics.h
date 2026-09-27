#pragma once
#include <vector>

// Nucleo fisico da simulacao de refracao em vidro.
// Cada vidro e representado por uma lente esferica (dioptro): a superficie e a
// esfera de raio r e o eixo optico aponta para a direcao (ax, ay, az).
// No plano principal (plano pelo centro perpendicular ao eixo) o vidro recorta
// um quadrado ou circulo de meia-largura R.  Isso permite representar desde uma
// janela (quase um vidro plano) ate uma lupa (esfera quase completa).
// A luz e tracada por ray tracing em 3D: os raios partem da fonte puntiforme,
// atravessam a bancada (2 vidros) e sao refratados pela lei de Snell.

enum GlassType {
    GT_WINDOW = 0, // Janela: vidro plano, praticamente nao desvia a luz
    GT_SHOWCASE, // Vitrine: placa grossa, deslocamento paralelo dos raios
    GT_LENS, // Lente biconvexa: converge os raios
    GT_MAGNIFIER, // Lupa: lente espessa de grande poder de aumento
    GT_BOTTLE, // Garrafa: corpo cilindrico de vidro
    GT_PRISM, // Prisma: face inclinada que separa as cores (dispersao)
    GT_TYPE_COUNT
};

// Indices dos meios opticos usados por TraceScene.
enum MediumIndex {
    MED_AIR_IN = 0,
    MED_GLASS_A = 1,
    MED_AIR_MID = 2,
    MED_GLASS_B = 3,
    MED_AIR_OUT = 4
};

// ---------------------------------------------------------------------------
//  Constantes da bancada optica
// ---------------------------------------------------------------------------
const float TABLE_Y = -0.95f;    // altura do tampo da mesa
const float TABLE_HALF = 3.25f;  // meia-largura da mesa

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct GlassObject {
    GlassType type = GT_LENS;
    float x = 0.0f, y = 0.0f, z = 0.0f; // centro geometrico na mesa
    float yaw = 0.0f; // rotacao em torno do eixo Y
    float width = 1.6f; // lado / diametro da placa de vidro
    float thickness = 0.55f; // espessura no centro
    float ior = 1.52f; // indice de refracao (vidro crown)
    bool square = false; // recorte quadrado (janela) ou circular
    bool active = true;
};

struct LightSource {
    float x = -2.4f, y = 1.05f, z = 0.35f;
    bool diverging = false;
    float color[3] = {1.0f, 1.0f, 1.0f};
};

struct GlassParams {
    std::vector<GlassObject> glasses;
    LightSource light;
    int maxReflections = 2; // reflexoes internas (reflexao total) seguidas por raio
};

//  Numeros de serie: (x, y) com y = 0 e a superficie de entrada.  A segunda
//  coordenada e sempre zero, portanto so os numeros parcialmente esfericos
//  (x, y, z) sao usados e o tracado 2D basta para resolver o raio meridional no
//  plano que contem o eixo optico.
struct RayResult {
    bool valid = false;
    float imageY = 0.0f; // altura do foco formado
    float objectY = 0.0f; // altura do objeto em relacao ao eixo
    float imageDistance = 0.0f; // distancia do foco ate a ultima superficie
    bool real = true; // foco real (a frente) ou virtual (atras)
};

// Material catalogado com os indices de refracao de tres comprimentos de onda,
// permitindo simular a dispersao cromatica (abertura do arco-iris) do prisma.
int MaterialCatalogSize();
const char* MaterialName(int index);
float MaterialIor(int index, int channel);

const char* GlassTypeName(GlassType type);

// Aplica dimensoes, recorte, rotacao e material tipicos de cada tipo de vidro.
void InitGlass(GlassObject& g, GlassType type, float x, float y, float z);

// Traz de volta a bancada inicial: uma lente no centro e uma janela inclinada.
GlassParams DefaultGlassParams();

// Numero de serie equivalente ao conjunto (na ordem em que aparecem na lista).
RayResult ComputeSystem(const GlassParams& params);

// Converte um clique na tela (coordenadas de janela com Y para cima) no ponto
// da mesa onde o novo vidro deve ser colocado.
bool ScreenToTable(int mx, int my, float* outX, float* outZ);

// Direcao do eixo optico (yaw em torno de Y).
Vec3 GlassAxis(const GlassObject& g);

// Desenha a marca do ponto da mesa sob o cursor (usada ao posicionar pecas).
void DrawPlacementCursor();

// Testa o raio (origem, direcao) contra o volume do vidro.
bool RayHitsGlass(const GlassObject& g, const Vec3& origin, const Vec3& dir, float maxDistance, float* outDistance);

// ---------------------------------------------------------------------------
//  Renderizacao
// ---------------------------------------------------------------------------
void DrawSphere(float radius, int stacks, int slices);

// Traco de um unico raio pela lista de vidros (refracao em cada face, com
// reflexoes internas quando ocorre reflexao total).
std::vector<Vec3> TraceScene(const GlassParams& params, const Vec3& origin, const Vec3& dir, float maxLength);