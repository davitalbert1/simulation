#include "render.h"
#include <GL/gl.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <functional>
#include <algorithm>
#include <GL/glu.h>

std::vector<Planet> planets;
std::vector<Sun> suns;
std::vector<Star> stars;
std::vector<Asteroid> asteroidBelt;
GLuint texRocky;
GLuint texIce;
GLuint texLava;
GLuint texGas;
GLuint texRing;

UniverseConfig CFG;
StarSystemType systemType;

GLfloat globalAmb[] = {0.03f, 0.03f, 0.04f, 1.0f};
GLfloat zero[] = {0,0,0,1};
GLfloat shininess = 1.0f;

StarSystemCenter systemCenter = {0.0f, 0.0f, 0.0f};

static const TerrainBand rockyBands[] = {
    {0.38f, {20, 40, 120}}, // oceano
    {0.45f, {194, 178, 128}}, // praia
    {0.70f, {50, 140, 60}}, // vegetação
    {0.85f, {100, 100, 100}}, // montanha
    {1.00f, {240, 240, 240}}  // neve
};

// método de Newton–Raphson: 𝐸𝑛+1 = (𝐸𝑛−𝐸𝑛−𝑒sin⁡(𝐸𝑛)−𝑀)/(1−𝑒cos⁡(𝐸𝑛))
float SolveKepler(float M, float e) {
    float E = M;
    for (int i = 0; i < 5; i++) E = E - (E - e * sinf(E) - M) / (1.0f - e * cosf(E));
    return E;
}

float BinaryStability(float distance, float starSeparation) {
    float ratio = distance / starSeparation;
    if (ratio < 1.5f) return 0.0f; // instável
    if (ratio < 3.0f) return 0.3f; // caótico
    if (ratio < 6.0f) return 0.7f; // parcialmente estável
    return 1.0f; // estável
}

// lei de Stefan–Boltzmann: 𝐿=(𝑇/𝑇0)^4
// lei do inverso do quadrado para intensidade/radiação: 𝐹 ∝ 𝐿/𝑑^2
float ComputeFlux(const Sun& sun, float distance) {
    float luminosity = powf(sun.temperature / CFG.starBaseTemp, 4.0f);
    distance = fmaxf(distance, sun.radius);
    return (luminosity * CFG.fluxFalloff * 35.0f) / (distance * distance);
}

// vermelho para temperaturas altas: R=329.6987(t−60)^−0.1332
// vermelho para temperaturas baixas: G=99.4708ln(t)−161.1196
// vermelho para temperaturas altas: 𝐺=288.1222(𝑡−60)−0.0755G=288.1222(t−60)^−0.0755
// azul para temperaturas intermediária: B=138.5177ln(t−10)−305.0448
Color BlackBodyColor(float temp) {
    float t = temp / 100.0f;
    float r, g, b;

    if (t <= 66) r = 255;
    else r = 329.698727446f * powf(t - 60, -0.1332047592f);

    if (t <= 66) g = 99.4708025861f * logf(t) - 161.1195681661f;
    else g = 288.1221695283f * powf(t - 60, -0.0755148492f);

    if (t >= 66) b = 255;
    else if (t <= 19) b = 0;
    else b = 138.5177312231f * logf(t - 10) - 305.0447927307f;

    auto clamp = [](float v) {
        return v < 0 ? 0 : (v > 255 ? 255 : v);
    };

    return {clamp(r)/255.0f, clamp(g)/255.0f, clamp(b)/255.0f};
}

// interpolação linear: 𝐿(𝑎,𝑏,𝑡)=𝑎+𝑡(𝑏−𝑎)
float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Derivada, Normalização, polinômio cúbico
//S(x)=3(edge1−edge0x−edge0​)^2−2(edge1−edge0x−edge0​)^3
float SmoothStep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float Fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// transição suave: t^3*(t(6t−15)+10)=t3(6t2−15t+10)=6t5−15t4+10t3
float Random2D(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}

float ToonShade(float v) {
    if (v < 0.20f) return 0.15f;
    if (v < 0.50f) return 0.45f;
    if (v < 0.80f) return 0.75f;

    return 1.0f;
}

float QuantizeColor(float v) {
    if (v < 0.33f) return 0.2f;
    if (v < 0.66f) return 0.6f;

    return 1.0f;
}

GLuint CreateTexture(int w, int h, GLenum format, const void* data) {
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    gluBuild2DMipmaps(GL_TEXTURE_2D, format, w, h, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return tex;
}

float PerlinNoise(float x, float y) {
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float sx = Fade(x - x0);
    float sy = Fade(y - y0);

    float n0 = Random2D(x0, y0);
    float n1 = Random2D(x1, y0);
    float ix0 = Lerp(n0, n1, sx);

    n0 = Random2D(x0, y1);
    n1 = Random2D(x1, y1);
    float ix1 = Lerp(n0, n1, sx);

    return Lerp(ix0, ix1, sy);
}

float FBM(float x, float y, int octaves = 3) {
    float value = 0.0f;

    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int i = 0; i < octaves; i++) {
        value += PerlinNoise(x * frequency, y * frequency) * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return value;
}

void SetMaterial(float r,float g,float b) {
    GLfloat diff[] = {r,g,b,1};
    GLfloat amb[]  = {r * CFG.amb, g * CFG.amb, b * CFG.amb, 1};
    GLfloat spec[] = {CFG.spec, CFG.spec, CFG.spec, 1.0f};

    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);

    GLfloat emission[] = {0,0,0,1};
    glMaterialfv(GL_FRONT, GL_EMISSION, emission);
}

void SetupLight(int index, const Sun& sun) {
    GLenum lightId = GL_LIGHT0 + index;

    glEnable(GL_LIGHTING);
    glEnable(lightId);

    GLfloat diff[] = {
        sun.color.r * CFG.starEmissionBoost,
        sun.color.g * CFG.starEmissionBoost,
        sun.color.b * CFG.starEmissionBoost,
        1.0f
    };

    GLfloat amb[]  = {0.0f, 0.0f, 0.0f, 1.0f};

    GLfloat spec[] = {
        sun.color.r * CFG.starEmissionBoost,
        sun.color.g * CFG.starEmissionBoost,
        sun.color.b * CFG.starEmissionBoost,
        1.0f
    };

    glLightfv(lightId, GL_DIFFUSE, diff);
    glLightfv(lightId, GL_AMBIENT, amb);
    glLightfv(lightId, GL_SPECULAR, spec);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    GLfloat pos[] = {sun.x, sun.y, sun.z, 1.0f};

    glLightfv(lightId, GL_POSITION, pos);

    glPopMatrix();

    glLightf(lightId, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(lightId, GL_LINEAR_ATTENUATION, 0.0f);
    glLightf(lightId, GL_QUADRATIC_ATTENUATION, CFG.lightAttenuation);
}

using PixelFunc = std::function<void(float noise, unsigned char& r, unsigned char& g, unsigned char& b)>;
using TextureShader = std::function<Color(float,const Planet*)>;

GLuint GenerateTextureBase(int size, PixelFunc pixelFunc) {
    std::vector<unsigned char> data(size * size * 3);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float u = (float)x / size;
            float v = (float)y / size;

            float theta = u * 2.0f * M_PI;
            float phi = v * M_PI;

            float nx = sinf(phi) * cosf(theta);
            float ny = cosf(phi);
            float nz = sinf(phi) * sinf(theta);

            float noise = FBM(nx * CFG.noiseScale, ny * CFG.noiseScale, 2); // FBM procedural
            noise = (noise + 1.0f) * 0.5f; // normaliza para 0..1
            noise = std::clamp(noise, 0.0f, 1.0f);

            unsigned char r, g, b;

            pixelFunc(noise, r, g, b);

            int i = (y * size + x) * 3;

            data[i + 0] = r;
            data[i + 1] = g;
            data[i + 2] = b;
        }
    }

    return CreateTexture(size, size, GL_RGB, data.data());
}

GLuint GenerateSunTexture(int size) {
    std::vector<unsigned char> data(size * size * 3);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = x / (float)size;
            float fy = y / (float)size;

            float dx = fx - 0.5f;
            float dy = fy - 0.5f;

            float d = sqrtf(dx * dx + dy * dy);

            // brilho
            float intensity = 1.0f - d;
            if (intensity < 0.0f) intensity = 0.0f;

            // manchas
            float plasma = sinf(fx * 12.0f) * cosf(fy * 12.0f);
            plasma = (plasma + 1.0f) * 0.5f;

            intensity += plasma * 0.25f;

            // quantização
            if (intensity < 0.25f) intensity = 0.2f;
            else if (intensity < 0.5f) intensity = 0.45f;
            else if (intensity < 0.75f) intensity = 0.75f;
            else intensity = 1.0f;

            // borda estilo cel shading
            float edge = 1.0f - SmoothStep(0.35f, 0.5f, d);
            intensity *= edge;

            unsigned char r = (unsigned char)(255 * intensity);
            unsigned char g = (unsigned char)(190 * intensity);
            unsigned char b = (unsigned char)(60 * intensity);

            int i = (y * size + x) * 3;

            data[i + 0] = r;
            data[i + 1] = g;
            data[i + 2] = b;
        }
    }

    return CreateTexture(size, size, GL_RGB, data.data());
}

void DrawLowPolySphere(float radius) {
    const int stacks = 6;
    const int slices = stacks;

    for (int i = 0; i < stacks; i++) {
        float lat0 = M_PI * (-0.5f + (float)i / stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);

        float z0 = sinf(lat0);
        float zr0 = cosf(lat0);

        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        glBegin(GL_TRIANGLE_STRIP);

        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * M_PI * (float)j / slices;

            float x = cosf(lng);
            float y = sinf(lng);

            // vértice 1
            glNormal3f(x * zr0, y * zr0, z0);
            glTexCoord2f((float)j / slices, (float)i / stacks);

            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            // vértice 2
            glNormal3f(x * zr1, y * zr1, z1);
            glTexCoord2f((float)j / slices, (float)(i + 1) / stacks);

            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }

        glEnd();
    }
}

void DrawOutline(float radius) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glDisable(GL_LIGHTING);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glPolygonMode(GL_BACK, GL_LINE);

    glLineWidth(1.0f); // linha fina

    glColor3f(0.0f, 0.0f, 0.0f);

    DrawSphere(radius * 1.01f); // menos segmentos pra reduzir grid

    glPolygonMode(GL_BACK, GL_FILL);

    glCullFace(GL_BACK);

    glPopAttrib();
}

float KeplerOrbitSpeed(float semiMajor) {
    semiMajor = fmaxf(semiMajor, 0.5f);
    return 2.2f / powf(semiMajor, 1.5f);
}

void GenerateAsteroidField(Planet& p) {
    int count = 90 + rand() % 120;
    float beltWidth = p.semiMajor * 0.12f;

    if (beltWidth < 0.45f) beltWidth = 0.45f;
    if (beltWidth > 1.80f) beltWidth = 1.80f;

    p.asteroids.clear();

    for (int i = 0; i < count; i++) {
        Asteroid a = {};
        a.angle = (rand() % 360) * M_PI / 180.0f;
        a.distance = p.semiMajor + ((rand() % 1000) / 1000.0f - 0.5f) * beltWidth;
        a.height = ((rand() % 1000) / 1000.0f - 0.5f) * beltWidth * 0.12f;
        a.size = 0.025f + (rand() % 100) / 1800.0f;
        a.speed = KeplerOrbitSpeed(a.distance) * (0.90f + (rand() % 200) / 1000.0f);
        a.tilt = (rand() % 360);

        p.asteroids.push_back(a);
    }
}

void InitAsteroidField(Planet& p, float distance, bool outerBelt) {
    p.type = ASTEROID_FIELD;
    p.surfaceClass = NONE;
    p.size = 0.0f;
    p.distance = distance;
    p.speed = KeplerOrbitSpeed(distance);
    p.inclination = (rand() % 1200) / 100.0f - 6.0f;
    p.eccentricity = outerBelt ? 0.08f : 0.04f;
    p.semiMajor = distance;
    p.semiMinor = p.semiMajor * sqrtf(1.0f - p.eccentricity * p.eccentricity);
    p.texture = texRocky;
    p.r = 0.35f;
    p.g = 0.35f;
    p.b = 0.35f;

    GenerateAsteroidField(p);
}

void GeneratePlanetaryRing(Planet& p) {
    p.asteroids.clear();
    float massFactor = p.size * p.size;
    float sizeFactor = std::clamp((p.size - 0.7f) / 0.8f, 0.0f, 1.0f);
    int count = 60 + (int)(massFactor * 90.0f);
    float innerRadius = p.size * (1.25f + sizeFactor * 0.15f);
    float outerRadius = p.size * (2.15f + sizeFactor * 0.45f);
    float ringWidth = outerRadius - innerRadius;
    float ringThickness = p.size * (0.04f + sizeFactor * 0.03f);

    p.ring.innerRadius = innerRadius;
    p.ring.outerRadius = outerRadius;

    for (int i = 0; i < count; i++) {
        Asteroid a = {};

        float radial = (rand() % 10000) / 10000.0f;
        radial = sqrtf(radial);

        a.angle = ((rand() % 10000) / 10000.0f) * 2.0f * M_PI;
        a.distance = innerRadius + radial * ringWidth;
        a.height = ((rand() % 10000) / 10000.0f - 0.5f) * ringThickness;
        a.size = 0.002f + (rand() % 1000) / 400000.0f;
        a.speed = 0.4f + (1.0f / (a.distance + 0.1f));
        a.tilt = ((rand() % 1000) / 1000.0f - 0.5f) * 8.0f;

        float eccentricity = ((rand() % 1000) / 1000.0f) * 0.03f;

        a.distance *= (1.0f + eccentricity);

        float normalized = (a.distance - innerRadius) / ringWidth;
        float band = sinf(normalized * 25.0f);

        if (band > 0.35f) continue;

        p.asteroids.push_back(a);
    }
}

void DrawRing(const Planet& p) {
    if (!p.ring.enabled) return;

    glPushMatrix();

    glDisable(GL_CULL_FACE);

    glRotatef(p.ring.tilt, 1, 0, 0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texRing);

    glDisable(GL_LIGHTING);

    glColor3f(1,1,1);

    int segments = 64;

    glBegin(GL_QUADS);

    for (int i = 0; i < segments; i++) {
        float a0 = (i / (float)segments) * 2.0f * M_PI;
        float a1 = ((i + 1) / (float)segments) * 2.0f * M_PI;

        float x0i = cosf(a0) * p.ring.innerRadius;
        float z0i = sinf(a0) * p.ring.innerRadius;

        float x0o = cosf(a0) * p.ring.outerRadius;
        float z0o = sinf(a0) * p.ring.outerRadius;

        float x1i = cosf(a1) * p.ring.innerRadius;
        float z1i = sinf(a1) * p.ring.innerRadius;

        float x1o = cosf(a1) * p.ring.outerRadius;
        float z1o = sinf(a1) * p.ring.outerRadius;

        float u0 = i / (float)segments;
        float u1 = (i + 1) / (float)segments;

        glTexCoord2f(u0, 0);
        glVertex3f(x0i, 0, z0i);
        glTexCoord2f(u0, 1);
        glVertex3f(x0o, 0, z0o);
        glTexCoord2f(u1, 1);
        glVertex3f(x1o, 0, z1o);
        glTexCoord2f(u1, 0);
        glVertex3f(x1i, 0, z1i);
    }

    glEnd();

    glEnable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);

    glPopMatrix();
}

void GenerateAsteroidBelt(float innerRadius, float outerRadius, int count) {
    asteroidBelt.clear();

    float beltWidth = outerRadius - innerRadius;
    beltWidth *= 2.5f;

    float beltThickness = beltWidth * 0.35f;

    for (int i = 0; i < count; i++) {
        Asteroid a = {};

        float radial = (rand() % 10000) / 10000.0f;
        radial = sqrtf(radial);

        a.angle = ((rand() % 10000) / 10000.0f) * 2.0f * M_PI;
        a.distance = innerRadius + radial * beltWidth;
        a.height = ((rand() % 10000) / 10000.0f - 0.5f) * beltThickness;
        a.size = 0.02f + (rand() % 1000) / 12000.0f;
        a.speed = KeplerOrbitSpeed(a.distance) * (0.90f + (rand() % 200) / 1000.0f);
        a.tilt = ((rand() % 1000) / 1000.0f - 0.5f) * 10.0f;

        asteroidBelt.push_back(a);
    }
}

void InitStars(int count) {
    stars.clear();

    srand(1337);

    for (int i = 0; i < count; i++) {
        float theta = (rand() % 360) * M_PI / 180.0f;
        float phi = (rand() % 180) * M_PI / 180.0f;

        float r = 1.0f;

        Star s;
        s.x = sinf(phi) * cosf(theta) * r;
        s.y = cosf(phi) * r;
        s.z = sinf(phi) * sinf(theta) * r;

        s.brightness = 0.3f + (rand() % 100) / 100.0f;

        stars.push_back(s);
    }
}

void InitSuns() {
    suns.clear();

    float roll = (rand() % 1000) / 1000.0f;

    if (roll < 0.70f) systemType = SINGLE;
    else if (roll < 0.95f) systemType = BINARY;
    else systemType = TRIPLE;

    // estrela primária
    Sun s1 = {};
    s1.x = 0;
    s1.y = 0;
    s1.z = 0;
    s1.radius = 1.0f;
    s1.mass = 1.0f;
    s1.temperature = 5778;
    s1.texture = GenerateSunTexture(256);
    s1.color = BlackBodyColor(s1.temperature);

    suns.push_back(s1);

    if (systemType == BINARY || systemType == TRIPLE) {
        Sun s2 = {};
        float angle = ((rand() % 360) / 180.0f) * M_PI;
        float r = 1.0f + (rand() % 300) / 100.0f;

        s2.x = cosf(angle) * r;
        s2.y = 0.0f;
        s2.z = sinf(angle) * r;

        s2.radius = 0.6f + (rand() % 40) / 100.0f;
        s2.mass = s2.radius;
        s2.temperature = 3000 + rand() % 4000;
        s2.texture = GenerateSunTexture(256);
        s2.color = BlackBodyColor(s2.temperature);

        suns.push_back(s2);
    }

    if (systemType == TRIPLE) {
        Sun s3 = {};
        float angle = ((rand() % 360) / 180.0f) * M_PI;
        float r = 2.0f + (rand() % 400) / 100.0f;

        s3.x = cosf(angle) * r;
        s3.y = 0.0f;
        s3.z = sinf(angle) * r;

        s3.radius = 0.4f + (rand() % 30) / 100.0f;
        s3.mass = s3.radius;
        s3.temperature = 2500 + rand() % 5000;
        s3.texture = GenerateSunTexture(256);
        s3.color = BlackBodyColor(s3.temperature);

        suns.push_back(s3);
    }
}

Color ShadeIce(float n, const Planet*) {
    return {0.6f, 0.9f, 1.0f};
}

Color ShadeLava(float n, const Planet*) {
    if (n < 0.4f) return {30 / 255.0f, 30 / 255.0f, 30 / 255.0f};
    return {1.0f, 0.35f, 0.0f};
}

Color ShadeGas(float n, const Planet*) {
    float bands = sinf(n * 40.0f) * 0.5f + sinf(n * 90.0f) * 0.25f;
    float v = (bands + 1.0f) * 0.5f;
    v = floorf(v * 4.0f) / 4.0f;

    return {(150.0f + v * 80.0f) / 255.0f, (100.0f + v * 60.0f) / 255.0f, (180.0f + v * 70.0f) / 255.0f};
}

Color ShadeRock(float n, const Planet*) {
    n = floorf(n * 4.0f) / 4.0f;
    if (n < 0.25f) return {0.22f, 0.20f, 0.18f};
    if (n < 0.50f) return {0.38f, 0.34f, 0.30f};
    if (n < 0.75f) return {0.52f, 0.48f, 0.42f};
    return {0.72f, 0.68f, 0.62f};
}

Color ShadeHabitable(float n, const Planet* p) {
    if (!p) return {1.0f, 1.0f, 1.0f};

    if (n < p->oceanLevel) return {0.0f, 0.45f, 1.0f}; // oceano
    if (n < p->oceanLevel + 0.03f) return {194.0f / 255.0f, 178.0f / 255.0f, 128.0f / 255.0f}; // praia

    // vegetação
    if (n < 0.72f) {
        float biome = FBM(n * 12.0f, p->humidity * 8.0f);
        biome = (biome + 1.0f) * 0.5f;

        if (biome < 0.35f) return {110.0f / 255.0f, 105.0f / 255.0f, 95.0f / 255.0f}; // seco
        if (biome < 0.55f) return {90.0f / 255.0f, 120.0f / 255.0f, 60.0f / 255.0f}; // vegetação média

        return {0.1f, 0.85f, 0.2f};// floresta
    }

    if (n < 0.90f) return {110.0f / 255.0f, 105.0f / 255.0f, 95.0f / 255.0f}; // montanhas

    return { 240.0f / 255.0f, 240.0f / 255.0f, 240.0f / 255.0f}; // neve
}

GLuint GeneratePlanetTexture(int size, TextureShader shader, const Planet* p = nullptr) {
    return GenerateTextureBase(size, [shader, p]( float n, unsigned char& r, unsigned char& g, unsigned char& b) {
        Color c = shader(n, p);

        auto Quantize = [](float v) {
            return QuantizeColor(v);
        };

        c.r = Quantize(c.r);
        c.g = Quantize(c.g);
        c.b = Quantize(c.b);

        auto clamp = [](float v) -> unsigned char {
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            return (unsigned char)(v * 255.0f);
        };

        r = clamp(c.r);
        g = clamp(c.g);
        b = clamp(c.b);
    });
}

void InitPlanetTextures() {
    texRocky = GeneratePlanetTexture(128, ShadeRock);
    texIce = GeneratePlanetTexture(128, ShadeIce);
    texLava = GeneratePlanetTexture(128, ShadeLava);
    texGas = GeneratePlanetTexture(128, ShadeGas);
}

void InitPlanets(int planetCount) {
    planets.clear();

    float currentDistance = CFG.binaryOrbitRadius + CFG.largestStarRadius + CFG.planetSpacingBase;
    int mainBeltSlot = -1;
    int outerBeltSlot = -1;

    // Frequencia inspirada no Sistema Solar: muitos planetas, um cinturao principal
    // perto da transicao rochoso/gasoso e, mais raramente, um cinturao externo.
    if (planetCount >= 5 && rand() % 100 < 85) mainBeltSlot = 4;
    if (planetCount >= 8 && rand() % 100 < 35) outerBeltSlot = planetCount - 1;

    for (int i = 0; i < planetCount; i++) {
        if (i == mainBeltSlot || i == outerBeltSlot) {
            Planet belt = {};
            bool outerBelt = i == outerBeltSlot;

            InitAsteroidField(belt, currentDistance, outerBelt);
            planets.push_back(belt);

            currentDistance += (outerBelt ? 2.0f : 1.2f) + (rand() % 120) / 100.0f;
            continue;
        }

        Planet p = {};
        p.size = 0.3f;
        p.distance = currentDistance;
        p.speed = KeplerOrbitSpeed(currentDistance) * (0.90f + (rand() % 200) / 1000.0f);
        p.inclination = (rand() % 3000) / 100.0f - 15.0f;
        p.eccentricity = (rand() % 25) / 100.0f;
        p.semiMajor = currentDistance;
        p.semiMinor = p.semiMajor * sqrtf(1.0f - p.eccentricity * p.eccentricity);
        p.humidity = (rand() % 100) / 100.0f;

        float flux = 0.0f;

        // posição temporária baseada em órbita média (aproximação)
        float planetPosX = currentDistance;
        float planetPosZ = 0.0f;

        for (auto& s : suns) {
            float dx = s.x - planetPosX;
            float dz = s.z - planetPosZ;
            float dist = sqrtf(dx*dx + dz*dz);

            flux += ComputeFlux(s, dist);
        }

        p.temperature = CFG.planetBaseTempFactor * powf(flux, CFG.fluxTemperatureExponent);

        bool beforeMainBelt = mainBeltSlot >= 0 && i < mainBeltSlot;
        float gasChance = beforeMainBelt ? 0.03f : 0.15f + (currentDistance * 0.02f);
        if (gasChance > 0.65f) gasChance = 0.65f;

        float rollGas = (rand() % 1000) / 1000.0f;

        if (rollGas < gasChance && currentDistance > 4.0f) p.type = GAS;
        else if (p.temperature < CFG.iceThreshold) p.type = ICE;
        else if (p.temperature > CFG.lavaThreshold) p.type = LAVA;
        else p.type = ROCKY;

        if (p.type == ROCKY) {
            bool goldilocks = p.temperature > CFG.habitableMinTemp && p.temperature < CFG.habitableMaxTemp;
            bool waterPotential = p.humidity > 0.35f;

            float roll = (rand() % 1000) / 1000.0f;

            if (goldilocks && waterPotential && roll < 0.08f) p.surfaceClass = HABITABLE;
            else if (p.temperature >= 305.0f) p.surfaceClass = ARID_HOT;
            else if (p.temperature <= 185.0f) p.surfaceClass = ARID_FROZEN;
            else p.surfaceClass = ARID_DRY;
        } else {
            p.surfaceClass = NONE;
        }

        if(p.type == GAS) p.size *= 2.5f + rand() % 200 / 100.0f;
        else p.size = 0.2f + (rand() % 50) / 100.0f;

        if (p.surfaceClass == HABITABLE) p.oceanLevel = 0.25f + ((rand() % 15) / 100.0f);
        else p.oceanLevel = 0.35f + ((rand() % 20) / 100.0f);

        if (p.type == ASTEROID_FIELD) GenerateAsteroidField(p);

        switch (p.surfaceClass) {
            case HABITABLE:
                p.r = 0.6f;
                p.g = 0.8f;
                p.b = 0.6f;
                break;
            case ARID_DRY:
                p.r = 0.6f;
                p.g = 0.5f;
                p.b = 0.4f;
                break;
            case ARID_HOT:
                p.r = 0.8f;
                p.g = 0.4f;
                p.b = 0.2f;
                break;
            case ARID_FROZEN:
                p.r = 0.7f;
                p.g = 0.8f;
                p.b = 0.9f;
                break;
        }

        switch (p.type) {
            case ROCKY:
                if (p.surfaceClass == HABITABLE) {
                    p.texture = GeneratePlanetTexture(256, ShadeHabitable, &p);
                    p.r = 0.6f;
                    p.g = 0.7f;
                    p.b = 0.5f;
                } else {
                    p.texture = GeneratePlanetTexture(256, ShadeRock, &p);
                    p.r = 0.5f;
                    p.g = 0.5f;
                    p.b = 0.5f;
                }
                break;
            case ICE:
                p.texture = GeneratePlanetTexture(256, ShadeIce, &p);
                p.r = 0.7f;
                p.g = 0.9f;
                p.b = 1.0f;
                break;
            case LAVA:
                p.texture = GeneratePlanetTexture(256, ShadeLava, &p);
                p.r = 1.0f;
                p.g = 0.4f;
                p.b = 0.1f;
                break;
            case GAS:
                p.texture = GeneratePlanetTexture( 256, ShadeGas, &p);

                p.r = 0.6f;
                p.g = 0.6f;
                p.b = 1.0f;

                if (rand() % 100 < 70) {
                    p.ring.enabled = true;
                    p.ring.tilt = (rand() % 80) - 40.0f;
                    GeneratePlanetaryRing(p);
                } else {
                    p.ring.enabled = false;
                }
                break;
            case ASTEROID_FIELD:
                p.texture = texRocky;
                p.r = 0.35f;
                p.g = 0.35f;
                p.b = 0.35f;

                break;
        }

        currentDistance += p.size * 2.5f + (rand() % 150) / 100.0f;

        int moonCount = rand() % 4;

        for (int m = 0; m < moonCount; m++) {
            Moon moon = {};
            moon.size = 0.05f + (rand() % 20) / 200.0f;
            moon.distance = 0.25f + (rand() % 40) / 100.0f;
            moon.speed = 1.5f + (rand() % 300) / 100.0f;
            moon.angleOffset = rand() % 360;
            moon.texture = texRocky;
            p.moons.push_back(moon);
        }

        planets.push_back(p);
    }
}

void DrawStars() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glPointSize(4.5f);
    glBegin(GL_POINTS);

    for (auto& s : stars) {
        float c = s.brightness;
        glColor3f(c, c, c * 1.2f);
        glVertex3f(s.x * CFG.starLight, s.y * CFG.starLight, s.z * CFG.starLight);
    }

    glEnd();
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    glPopAttrib();
}

void DrawSphere(float radius) {
    const int stacks = 64;
    const int slices = stacks;

    for (int i = 0; i < stacks; i++) {
        float lat0 = M_PI * (-0.5f + (float)i / stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);

        float z0 = sinf(lat0);
        float zr0 = cosf(lat0);

        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        float v0 = (float)i / stacks;
        float v1 = (float)(i + 1) / stacks;

        glBegin(GL_QUAD_STRIP);

        for (int j = 0; j <= slices; j++) {
            float lng = 2 * M_PI * (float)j / slices;

            float x = cosf(lng);
            float y = sinf(lng);

            float u = (float)j / slices;

            glNormal3f(x * zr0, y * zr0, z0);
            glTexCoord2f(u, v0);
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);
            glNormal3f(x * zr1, y * zr1, z1);
            glTexCoord2f(u, v1);
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}

void DrawSolarSystem(float time) {
    glEnable(GL_NORMALIZE);

    // SISTEMA NO MESMO ESPAÇO
    glPushMatrix();
    glTranslatef(systemCenter.x, systemCenter.y, systemCenter.z);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // CONFIGURA TEXTURA + MATERIAL
    glDisable(GL_LIGHTING); // PRIMEIRO DESENHA ESTRELAS SEM LIGHTING

    if (suns.size() == 2) {
        float totalMass = suns[0].mass + suns[1].mass;
        float r1 = (suns[1].mass / totalMass) * CFG.binaryOrbitRadius;
        float r2 = (suns[0].mass / totalMass) * CFG.binaryOrbitRadius;
        float binaryOrbit = time * CFG.binaryOrbitSpeed;

        suns[0].x = cosf(binaryOrbit) * r1;
        suns[0].z = sinf(binaryOrbit) * r1;
        suns[0].y = 0.0f;

        suns[1].x = -cosf(binaryOrbit) * r2;
        suns[1].z = -sinf(binaryOrbit) * r2;
        suns[1].y = 0.0f;
    }

    for (size_t i = 0; i < suns.size(); i++) {
        auto& sun = suns[i];

        glPushMatrix();

        glTranslatef(sun.x, sun.y, sun.z);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, sun.texture);

        GLfloat emission[] = {
            sun.color.r * CFG.starEmissionBoost,
            sun.color.g * CFG.starEmissionBoost,
            sun.color.b * CFG.starEmissionBoost,
            1.0f
        };

        glMaterialfv(GL_FRONT, GL_EMISSION, emission);

        glColor3f(1,1,1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glDisable(GL_LIGHTING);

        glColor4f(sun.color.r, sun.color.g, sun.color.b, 0.25f);

        DrawSphere(sun.radius * 1.25f);

        glDisable(GL_BLEND);

        glEnable(GL_LIGHTING);

        DrawSphere(sun.radius);

        glMaterialfv(GL_FRONT, GL_EMISSION, zero);

        glDisable(GL_TEXTURE_2D);

        glPopMatrix();
    }

    // LUZES
    for (size_t i = 0; i < suns.size(); i++) SetupLight((int)i, suns[i]);

    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);

    for (auto& p : planets) {
        glPushMatrix();

        // INCLINAÇÃO ORBITAL
        glRotatef(p.inclination, 0, 0, 1);

        // ÓRBITA
        float timeScale = CFG.planetOrbitTimeScale;
        float t = time * timeScale * p.speed;

        float meanAnomaly = t;
        float E = SolveKepler(meanAnomaly, p.eccentricity);
        float x = p.semiMajor * (cosf(E) - p.eccentricity);
        float z = p.semiMinor * sinf(E);

        if (p.type == ASTEROID_FIELD) {
            glDisable(GL_LIGHTING);
            glBindTexture(GL_TEXTURE_2D, texRocky);
            glEnable(GL_TEXTURE_2D);

            for (auto& a : p.asteroids) {
                float asteroidTime = time * a.speed + a.angle;
                float depthScale = 0.7f + 0.3f * sinf(asteroidTime);
                float ax = cosf(asteroidTime) * a.distance * depthScale;
                float az = sinf(asteroidTime) * a.distance * depthScale;

                glPushMatrix();
                glTranslatef(ax, a.height, az);
                glRotatef(a.tilt + time * 20.0f, 1, 1, 0);

                float s = a.size;
                float c = 0.3f + (a.size * 20.0f);

                if (c > 1.0f) c = 1.0f;
                if (c < 0.15f) c = 0.15f;

                float brightness = ToonShade(c);

                glColor3f(brightness * 0.8f, brightness * 0.75f, brightness * 0.7f);

                glPushMatrix();

                float deformX = 0.8f + sinf(a.angle * 2.7f) * 0.3f;
                float deformY = 0.8f + cosf(a.angle * 4.1f) * 0.3f;
                float deformZ = 0.8f + sinf(a.angle * 7.9f) * 0.3f;

                glScalef(deformX, deformY, deformZ);

                DrawLowPolySphere(s);

                glPopMatrix();

                glPopMatrix();
            }

            glPointSize(1.5f);
            glBegin(GL_POINTS);

            for (int i = 0; i < 80; i++) {
                float angle = i * 13.37f;
                float radius = p.semiMajor + (rand() % 200) / 500.0f - 0.2f;

                float asteroidTime = time * 0.2f + angle;

                float ax = cosf(asteroidTime) * radius;
                float az = sinf(asteroidTime) * radius;

                float flicker = 0.2f + 0.2f * sinf(time * 2.0f + i);

                glColor3f(flicker * 0.6f, flicker * 0.6f, flicker * 0.7f);
                glVertex3f(ax, 0.02f * sinf(i), az);
            }

            glEnd();

            glDisable(GL_TEXTURE_2D);
            glEnable(GL_LIGHTING);
            glPopMatrix();

            continue;
        }

        glTranslatef(x, 0.0f, z);

        // ROTAÇÃO DO PLANETA
        glRotatef(t * 20.0f, 0, 1, 0);

        // ILUMINAÇÃO DAS ESTRELAS
        float totalFlux = 0.0f;
        for (auto& s : suns) {
            float dx = s.x - x;
            float dz = s.z - z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist < 0.1f) dist = 0.1f;
            totalFlux += ComputeFlux(s, dist);
        }

        // BRILHO BASEADO NA LUZ
        float illumination = powf(totalFlux, CFG.fluxIlluminationExponent);

        illumination = fmaxf(0.0f, illumination);
        illumination = fminf(illumination, 1.0f);

        illumination = ToonShade(illumination);

        float lit = illumination;

        SetMaterial(fminf(p.r * lit, 1.0f), fminf(p.g * lit, 1.0f), fminf(p.b * lit, 1.0f));

        // TEMPERATURA
        float temp = CFG.planetBaseTempFactor * powf(totalFlux, CFG.fluxTemperatureExponent);

        // EMISSÃO
        GLfloat emission[] = {
            p.r * illumination * CFG.emissionBase,
            p.g * illumination * CFG.emissionBase,
            p.b * illumination * CFG.emissionBase,
            1.0f
        };

        // LAVA BRILHA
        if (p.type == LAVA) {
            emission[0] *= CFG.lavaEmissionBoost;
            emission[1] *= CFG.lavaEmissionBoost;
        }

        glMaterialfv(GL_FRONT, GL_EMISSION, emission);

        // TEXTURA
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, p.texture);

        glColor3f(1.0f, 1.0f, 1.0f);

        // DESENHA
        DrawSphere(p.size);

        // ATMOSFERA
        if (p.habitable) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDisable(GL_LIGHTING);
            glColor4f(0.2f, 0.7f, 1.0f, 0.5f);
            DrawSphere(p.size * CFG.atmosphereScale);
            glEnable(GL_LIGHTING);
            glDisable(GL_BLEND);
        }

        DrawRing(p);
        glDisable(GL_TEXTURE_2D);

        // LUAS
        for (auto& m : p.moons) {
            glPushMatrix();

            float mt = time * m.speed + m.angleOffset;
            float mx = cosf(mt) * m.distance;
            float mz = sinf(mt) * m.distance;

            glTranslatef(mx, 0, mz);

            SetMaterial(CFG.moonIlumination * illumination, CFG.moonIlumination * illumination, CFG.moonIlumination * illumination
            );
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, m.texture);
            DrawSphere(m.size);
            glDisable(GL_TEXTURE_2D);
            glPopMatrix();
        }

        glMaterialfv(GL_FRONT, GL_EMISSION, zero);
        glPopMatrix();
    }

    glDisable(GL_LIGHTING);
    glPopMatrix();
}

// =======================================================
// SERIALIZAÇÃO E PARSING DE JSON PARA O SISTEMA ESTELAR
// =======================================================
#include <string>
#include <cstdio>
#include <cstring>

struct JsonVal {
    enum Type { NIL, NUM, STR, BOOL, OBJ, ARR };
    Type type = NIL;
    double numVal = 0.0;
    std::string strVal;
    bool boolVal = false;
    std::vector<std::pair<std::string, JsonVal>> objVal;
    std::vector<JsonVal> arrVal;

    JsonVal get(const std::string& key) const {
        if (type == OBJ) {
            for (const auto& pair : objVal) {
                if (pair.first == key) return pair.second;
            }
        }
        return JsonVal();
    }
};

struct ParseState {
    const char* p;
    void skipWhitespace() {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',' || *p == ':')) {
            p++;
        }
    }
};

bool isDigitChar(char c) {
    return c >= '0' && c <= '9';
}

JsonVal parseJsonValue(ParseState& s);

std::string parseJsonString(ParseState& s) {
    s.skipWhitespace();
    if (*s.p != '"') return "";
    s.p++;
    const char* start = s.p;
    while (*s.p && *s.p != '"') {
        s.p++;
    }
    std::string str(start, s.p);
    if (*s.p == '"') s.p++;
    return str;
}

JsonVal parseJsonObject(ParseState& s) {
    JsonVal v;
    v.type = JsonVal::OBJ;
    s.skipWhitespace();
    if (*s.p != '{') return v;
    s.p++;
    
    while (true) {
        s.skipWhitespace();
        if (*s.p == '}') {
            s.p++;
            break;
        }
        std::string key = parseJsonString(s);
        s.skipWhitespace();
        JsonVal val = parseJsonValue(s);
        v.objVal.push_back({key, val});
        s.skipWhitespace();
        if (*s.p == ',') s.p++;
        else if (*s.p == '}') {
            s.p++;
            break;
        } else if (*s.p == '\0') {
            break;
        }
    }
    return v;
}

JsonVal parseJsonArray(ParseState& s) {
    JsonVal v;
    v.type = JsonVal::ARR;
    s.skipWhitespace();
    if (*s.p != '[') return v;
    s.p++;
    
    while (true) {
        s.skipWhitespace();
        if (*s.p == ']') {
            s.p++;
            break;
        }
        JsonVal val = parseJsonValue(s);
        v.arrVal.push_back(val);
        s.skipWhitespace();
        if (*s.p == ',') s.p++;
        else if (*s.p == ']') {
            s.p++;
            break;
        } else if (*s.p == '\0') {
            break;
        }
    }
    return v;
}

JsonVal parseJsonValue(ParseState& s) {
    s.skipWhitespace();
    JsonVal v;
    if (*s.p == '"') {
        v.type = JsonVal::STR;
        v.strVal = parseJsonString(s);
    } else if (*s.p == '{') {
        return parseJsonObject(s);
    } else if (*s.p == '[') {
        return parseJsonArray(s);
    } else if (*s.p == 't' || *s.p == 'f') {
        v.type = JsonVal::BOOL;
        if (strncmp(s.p, "true", 4) == 0) {
            v.boolVal = true;
            s.p += 4;
        } else {
            v.boolVal = false;
            s.p += 5;
        }
    } else if (isDigitChar(*s.p) || *s.p == '-' || *s.p == '.') {
        v.type = JsonVal::NUM;
        char* end;
        v.numVal = strtod(s.p, &end);
        s.p = end;
    }
    return v;
}

void SaveSystemToFile(const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (!f) return;
    fprintf(f, "{\n");
    fprintf(f, "  \"suns\": [\n");
    for (size_t i = 0; i < suns.size(); ++i) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"x\": %f,\n", suns[i].x);
        fprintf(f, "      \"y\": %f,\n", suns[i].y);
        fprintf(f, "      \"z\": %f,\n", suns[i].z);
        fprintf(f, "      \"radius\": %f,\n", suns[i].radius);
        fprintf(f, "      \"temperature\": %f,\n", suns[i].temperature);
        fprintf(f, "      \"mass\": %f,\n", suns[i].mass);
        fprintf(f, "      \"color\": {\"r\": %f, \"g\": %f, \"b\": %f}\n", suns[i].color.r, suns[i].color.g, suns[i].color.b);
        fprintf(f, "    }%s\n", (i == suns.size() - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"planets\": [\n");
    for (size_t i = 0; i < planets.size(); ++i) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"distance\": %f,\n", planets[i].distance);
        fprintf(f, "      \"speed\": %f,\n", planets[i].speed);
        fprintf(f, "      \"size\": %f,\n", planets[i].size);
        fprintf(f, "      \"r\": %f, \"g\": %f, \"b\": %f,\n", planets[i].r, planets[i].g, planets[i].b);
        fprintf(f, "      \"inclination\": %f,\n", planets[i].inclination);
        fprintf(f, "      \"semiMajor\": %f,\n", planets[i].semiMajor);
        fprintf(f, "      \"semiMinor\": %f,\n", planets[i].semiMinor);
        fprintf(f, "      \"eccentricity\": %f,\n", planets[i].eccentricity);
        fprintf(f, "      \"temperature\": %f,\n", planets[i].temperature);
        fprintf(f, "      \"humidity\": %f,\n", planets[i].humidity);
        fprintf(f, "      \"oceanLevel\": %f,\n", planets[i].oceanLevel);
        fprintf(f, "      \"habitable\": %s,\n", planets[i].habitable ? "true" : "false");
        fprintf(f, "      \"type\": %d,\n", (int)planets[i].type);
        fprintf(f, "      \"surfaceClass\": %d,\n", (int)planets[i].surfaceClass);
        fprintf(f, "      \"ring\": {\n");
        fprintf(f, "        \"innerRadius\": %f,\n", planets[i].ring.innerRadius);
        fprintf(f, "        \"outerRadius\": %f,\n", planets[i].ring.outerRadius);
        fprintf(f, "        \"tilt\": %f,\n", planets[i].ring.tilt);
        fprintf(f, "        \"enabled\": %s\n", planets[i].ring.enabled ? "true" : "false");
        fprintf(f, "      },\n");
        
        fprintf(f, "      \"moons\": [\n");
        for (size_t m = 0; m < planets[i].moons.size(); ++m) {
            fprintf(f, "        {\n");
            fprintf(f, "          \"size\": %f,\n", planets[i].moons[m].size);
            fprintf(f, "          \"distance\": %f,\n", planets[i].moons[m].distance);
            fprintf(f, "          \"speed\": %f,\n", planets[i].moons[m].speed);
            fprintf(f, "          \"angleOffset\": %f\n", planets[i].moons[m].angleOffset);
            fprintf(f, "        }%s\n", (m == planets[i].moons.size() - 1) ? "" : ",");
        }
        fprintf(f, "      ],\n");

        fprintf(f, "      \"asteroids\": [\n");
        for (size_t a = 0; a < planets[i].asteroids.size(); ++a) {
            fprintf(f, "        {\n");
            fprintf(f, "          \"angle\": %f,\n", planets[i].asteroids[a].angle);
            fprintf(f, "          \"distance\": %f,\n", planets[i].asteroids[a].distance);
            fprintf(f, "          \"size\": %f,\n", planets[i].asteroids[a].size);
            fprintf(f, "          \"speed\": %f,\n", planets[i].asteroids[a].speed);
            fprintf(f, "          \"tilt\": %f,\n", planets[i].asteroids[a].tilt);
            fprintf(f, "          \"height\": %f\n", planets[i].asteroids[a].height);
            fprintf(f, "        }%s\n", (a == planets[i].asteroids.size() - 1) ? "" : ",");
        }
        fprintf(f, "      ]\n");
        
        fprintf(f, "    }%s\n", (i == planets.size() - 1) ? "" : ",");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

void RebuildSystemTextures() {
    for (auto& s : suns) {
        s.texture = GenerateSunTexture(256);
    }
    for (auto& p : planets) {
        if (p.type == ROCKY) {
            if (p.surfaceClass == HABITABLE) {
                p.texture = GeneratePlanetTexture(256, ShadeHabitable, &p);
            } else {
                p.texture = GeneratePlanetTexture(256, ShadeRock, &p);
            }
        } else if (p.type == ICE) {
            p.texture = GeneratePlanetTexture(256, ShadeIce, &p);
        } else if (p.type == LAVA) {
            p.texture = GeneratePlanetTexture(256, ShadeLava, &p);
        } else if (p.type == GAS) {
            p.texture = GeneratePlanetTexture(256, ShadeGas, &p);
        } else if (p.type == ASTEROID_FIELD) {
            p.texture = texRocky;
        }
        
        for (auto& m : p.moons) {
            m.texture = texRocky;
        }
    }
}

bool LoadSystemFromFile(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(size, '\0');
    size_t readBytes = fread(&content[0], 1, size, f);
    fclose(f);
    if (readBytes == 0) return false;

    ParseState s;
    s.p = content.c_str();
    s.skipWhitespace();
    JsonVal root = parseJsonObject(s);

    JsonVal sunsVal = root.get("suns");
    JsonVal planetsVal = root.get("planets");

    if (sunsVal.type != JsonVal::ARR || planetsVal.type != JsonVal::ARR) {
        return false;
    }

    suns.clear();
    for (const auto& sv : sunsVal.arrVal) {
        Sun sun = {};
        sun.x = (float)sv.get("x").numVal;
        sun.y = (float)sv.get("y").numVal;
        sun.z = (float)sv.get("z").numVal;
        sun.radius = (float)sv.get("radius").numVal;
        sun.temperature = (float)sv.get("temperature").numVal;
        sun.mass = (float)sv.get("mass").numVal;
        JsonVal colorVal = sv.get("color");
        sun.color.r = (float)colorVal.get("r").numVal;
        sun.color.g = (float)colorVal.get("g").numVal;
        sun.color.b = (float)colorVal.get("b").numVal;
        suns.push_back(sun);
    }

    planets.clear();
    for (const auto& pv : planetsVal.arrVal) {
        Planet planet = {};
        planet.distance = (float)pv.get("distance").numVal;
        planet.speed = (float)pv.get("speed").numVal;
        planet.size = (float)pv.get("size").numVal;
        planet.r = (float)pv.get("r").numVal;
        planet.g = (float)pv.get("g").numVal;
        planet.b = (float)pv.get("b").numVal;
        planet.inclination = (float)pv.get("inclination").numVal;
        planet.semiMajor = (float)pv.get("semiMajor").numVal;
        planet.semiMinor = (float)pv.get("semiMinor").numVal;
        planet.eccentricity = (float)pv.get("eccentricity").numVal;
        planet.temperature = (float)pv.get("temperature").numVal;
        planet.humidity = (float)pv.get("humidity").numVal;
        planet.oceanLevel = (float)pv.get("oceanLevel").numVal;
        planet.habitable = pv.get("habitable").boolVal;
        planet.type = (PlanetType)(int)pv.get("type").numVal;
        planet.surfaceClass = (SurfaceClass)(int)pv.get("surfaceClass").numVal;

        JsonVal ringVal = pv.get("ring");
        planet.ring.innerRadius = (float)ringVal.get("innerRadius").numVal;
        planet.ring.outerRadius = (float)ringVal.get("outerRadius").numVal;
        planet.ring.tilt = (float)ringVal.get("tilt").numVal;
        planet.ring.enabled = ringVal.get("enabled").boolVal;

        JsonVal moonsVal = pv.get("moons");
        for (const auto& mv : moonsVal.arrVal) {
            Moon moon = {};
            moon.size = (float)mv.get("size").numVal;
            moon.distance = (float)mv.get("distance").numVal;
            moon.speed = (float)mv.get("speed").numVal;
            moon.angleOffset = (float)mv.get("angleOffset").numVal;
            planet.moons.push_back(moon);
        }

        JsonVal astVal = pv.get("asteroids");
        for (const auto& av : astVal.arrVal) {
            Asteroid ast = {};
            ast.angle = (float)av.get("angle").numVal;
            ast.distance = (float)av.get("distance").numVal;
            ast.size = (float)av.get("size").numVal;
            ast.speed = (float)av.get("speed").numVal;
            ast.tilt = (float)av.get("tilt").numVal;
            ast.height = (float)av.get("height").numVal;
            planet.asteroids.push_back(ast);
        }

        planets.push_back(planet);
    }

    RebuildSystemTextures();
    return true;
}

