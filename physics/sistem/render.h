#pragma once
#include <vector>
#include <GL/gl.h>

struct UniverseConfig {
    float binaryInstabilityScale = 8.0f;
    float starLuminosityBase = 5778.0f;
    float fluxFalloff = 2.5f;

    float lightAttenuation = 0.00015f;

    float planetSpacingBase = 2.5f;

    float habitableMinTemp = 185.0f;
    float habitableMaxTemp = 305.0f;

    float iceThreshold = 140.0f;
    float lavaThreshold = 450.0f;

    float atmosphereAlpha = 0.15f;

    float ringDensity = 0.6f;

    float noiseScale = 2.0f;

    float amb = 0.6f;
    float spec = 0.0f;

    float moonIlumination = 0.7f;
    float starLight = 1000.0f;

    float binaryOrbitRadius = 1.5f;
    float largestStarRadius = 1.0f;

    float starBaseTemp = 5778.0f;
    float planetBaseTempFactor = 278.0f;
    float fluxTemperatureExponent = 0.25f;
    float fluxIlluminationExponent = 0.30f;

    float emissionBase = 0.08f;

    float binaryOrbitSpeed = 0.15f;
    float planetOrbitTimeScale = 0.2f;

    float starEmissionBoost = 3.0f;
    float lavaEmissionBoost = 5.0f;

    float atmosphereScale = 1.05f;

    float starfieldDistanceScale = 1000.0f;
};

struct Color {
    float r, g, b;
};

enum StarSystemType {
    SINGLE,
    BINARY,
    TRIPLE
};

struct Star {
    float x, y, z;
    float brightness;
};

enum SurfaceClass {
    HABITABLE,
    ARID_DRY,
    ARID_HOT,
    ARID_FROZEN,
    NONE
};

enum PlanetType {
    ROCKY,
    ICE,
    LAVA,
    GAS,
    ASTEROID_FIELD
};

struct Moon {
    float size;
    float distance;
    float speed;
    float angleOffset;
    GLuint texture;
};

struct Ring {
    float innerRadius;
    float outerRadius;
    float tilt;
    GLuint texture;
    bool enabled;
};

struct Asteroid {
    float angle;
    float distance;
    float size;
    float speed;
    float tilt;
    float height;
};

struct Planet {
    float distance; // distância da órbita
    float speed; // velocidade orbital
    float size; // raio
    float r, g, b; // cor
    float inclination; // em graus
    float semiMajor; // raio maior (a)
    float semiMinor; // raio menor (b)
    float eccentricity;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float oceanLevel = 0.0f;

    bool habitable = false;

    GLuint texture;

    PlanetType type;
    Ring ring;
    SurfaceClass surfaceClass;
    std::vector<Asteroid> asteroids;
    std::vector<Moon> moons;
};

struct StarSystemCenter {
    float x, y, z;
};

struct Sun {
    float x, y, z;
    float radius;
    float temperature;
    float mass;

    GLuint texture;

    Color color;
};

struct ColorRGB {
    unsigned char r, g, b;
};

struct TerrainBand {
    float maxHeight;
    ColorRGB color;
};

enum TextureStyle {
    TEX_ROCK,
    TEX_ICE,
    TEX_LAVA,
    TEX_GAS,
    TEX_HABITABLE
};

extern std::vector<Planet> planets;
extern std::vector<Sun> suns;

void SaveSystemToFile(const char* filepath);
bool LoadSystemFromFile(const char* filepath);
void RebuildSystemTextures();

void InitPlanetTextures();
void InitSuns();
void InitPlanets(int count);
void GenerateAsteroidBelt(float innerRadius, float outerRadius, int count);
void DrawSphere(float radius);
void DrawSolarSystem(float time);