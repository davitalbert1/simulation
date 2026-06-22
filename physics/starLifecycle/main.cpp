#include "window.h"
#include <gl/gl.h>
#include <GL/glu.h>
#include <windows.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

// Pi constant
const float PI_F = 3.14159265f;

// Fonts
GLuint fontBaseRegular = 0;
GLuint fontBaseBold = 0;
GLuint fontBaseLarge = 0;

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
    HFONT fontLarge = CreateFont(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    SelectObject(g_hdc, fontLarge);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseLarge);

    SelectObject(g_hdc, oldFont);
    DeleteObject(fontReg);
    DeleteObject(fontBold);
    DeleteObject(fontLarge);
}

void PrintString(float x, float y, const char* str, GLuint base, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

// 2D primitives helpers
void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void DrawRectOutline(float x, float y, float w, float h, float r, float g, float b, float thickness = 1.0f) {
    glColor3f(r, g, b);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void DrawCircle(float cx, float cy, float r, float red, float green, float blue, int segments = 100) {
    glColor3f(red, green, blue);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float theta = 2.0f * PI_F * float(i) / float(segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

void DrawFilledCircle(float cx, float cy, float r, float red, float green, float blue, float alpha = 1.0f, int segments = 100) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float theta = 2.0f * PI_F * float(i) / float(segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

// Particle System
struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b, a;
    float size;
    float life;
    float maxLife;
};

std::vector<Particle> particles;

void UpdateParticles(float dt) {
    for (size_t i = 0; i < particles.size();) {
        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].life -= dt;
        particles[i].a = (particles[i].life / particles[i].maxLife) * 0.7f; // fade out
        
        if (particles[i].life <= 0.0f) {
            particles[i] = particles.back();
            particles.pop_back();
        } else {
            i++;
        }
    }
}

void DrawParticles() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto& p : particles) DrawFilledCircle(p.x, p.y, p.size, p.r, p.g, p.b, p.a, 8);
    glDisable(GL_BLEND);
}

// Simulation Parameters
double initialMass = 1.0; // 0.5, 1.0, 10.0, 30.0 M_sun
double agePercent = 0.0;  // 0.0 to 1.0
bool isRunning = true;
double simSpeed = 0.01;  // speed of aging

struct StarState {
    std::string stageName;
    int stageIndex; // 0 = Nebula, 1 = Protostar, 2 = Main Sequence, 3 = Giant/Supergiant, 4 = Transition/Nova, 5 = Remnant
    double temp;
    double lum;
    double radius;
    double ageYears; // in millions of years
};

std::vector<std::pair<double, double>> hrTrail; // historical (temp, lum)

// Interpolation Helper
StarState GetStellarProperties(double mass, double agePct) {
    StarState s;
    s.ageYears = 0.0;
    
    // Lifetimes (in millions of years)
    double lifetime = 10000.0; // 1M_sun = 10 billion years (10000M)
    if (mass < 0.8) lifetime = 50000.0; // low mass = 50 billion years
    else if (mass > 25.0) lifetime = 5.0; // very massive = 5 million years
    else if (mass > 8.0) lifetime = 30.0; // massive = 30 million years
    
    // Stage mapping
    // 0.0 to 0.08: Nebula
    // 0.08 to 0.15: Protostar
    // 0.15 to 0.70: Main Sequence
    // 0.70 to 0.83: Giant / Supergiant
    // 0.83 to 0.88: Explosion / Ejection (PN or Supernova)
    // 0.88 to 1.00: Remnant (White Dwarf, Neutron Star, Black Hole)
    
    double t = agePct;
    
    if (mass < 8.0) { // Low-medium mass track (e.g. 1.0 M_sun or 0.5 M_sun)
        s.ageYears = agePct * lifetime;
        
        if (t <= 0.08) {
            s.stageName = "Nébula Estelar (Berço)";
            s.stageIndex = 0;
            // Nebula collapse: starts cold, heats up slightly
            double local_t = t / 0.08;
            s.temp = 50.0 + local_t * 950.0; // 50 to 1000K
            s.lum = 1e-4 + local_t * 1e-2;
            s.radius = 10.0 - local_t * 8.0; // collapsing size
        }
        else if (t <= 0.15) {
            s.stageName = "Protoestrela";
            s.stageIndex = 1;
            // Protostar: glowing core
            double local_t = (t - 0.08) / 0.07;
            s.temp = 1000.0 + local_t * 2000.0; // 1000 to 3000K
            s.lum = 1e-2 + local_t * 5.0; // brightens
            s.radius = 2.0 - local_t * 1.0;
        }
        else if (t <= 0.70) {
            s.stageName = "Seqüência Principal";
            s.stageIndex = 2;
            // Main Sequence: stable fusing hydrogen
            double local_t = (t - 0.15) / 0.55;
            double msTemp = (mass < 0.8) ? 3800.0 : 5800.0;
            double msLum = (mass < 0.8) ? 0.08 : 1.0;
            double msRad = (mass < 0.8) ? 0.6 : 1.0;
            
            s.temp = msTemp + local_t * (msTemp * 0.1); // slight temp increase
            s.lum = msLum + local_t * (msLum * 0.3);
            s.radius = msRad + local_t * (msRad * 0.1);
        }
        else if (t <= 0.83) {
            s.stageName = "Gigante Vermelha";
            s.stageIndex = 3;
            // Expands massively, temp cools
            double local_t = (t - 0.70) / 0.13;
            double startTemp = (mass < 0.8) ? 4100.0 : 6200.0;
            double startLum = (mass < 0.8) ? 0.1 : 1.3;
            double startRad = (mass < 0.8) ? 0.65 : 1.1;
            
            s.temp = startTemp - local_t * (startTemp - 3100.0); // cools to 3100K
            s.lum = startLum + local_t * 1500.0; // swells in luminosity
            s.radius = startRad + local_t * 120.0; // expands to 120 solar radii
        }
        else if (t <= 0.88) {
            s.stageName = "Nebulosa Planetária";
            s.stageIndex = 4;
            // Ejecting shells, revealing hot core
            double local_t = (t - 0.83) / 0.05;
            s.temp = 3100.0 + local_t * 96900.0; // core temp revealed up to 100,000K
            s.lum = 1500.0 - local_t * 1400.0; // drops
            s.radius = 120.0 - local_t * 119.5; // size drops
        }
        else {
            s.stageName = "Anã Branca (Remanescente)";
            s.stageIndex = 5;
            // Cooling White Dwarf
            double local_t = (t - 0.88) / 0.12;
            s.temp = 100000.0 - local_t * 95000.0; // cools down to 5000K
            s.lum = 100.0 * pow(1.0 - local_t, 3) + 1e-4; // drops down to 10^-4
            s.radius = 0.01; // Earth size
        }
    }
    else { // High mass and very high mass tracks (e.g. 10.0 M_sun or 30.0 M_sun)
        s.ageYears = agePct * lifetime;
        
        if (t <= 0.08) {
            s.stageName = "Nébula Estelar (Berço)";
            s.stageIndex = 0;
            double local_t = t / 0.08;
            s.temp = 50.0 + local_t * 1450.0; // 50 to 1500K
            s.lum = 1e-3 + local_t * 1.0;
            s.radius = 20.0 - local_t * 12.0;
        }
        else if (t <= 0.15) {
            s.stageName = "Protoestrela Massiva";
            s.stageIndex = 1;
            double local_t = (t - 0.08) / 0.07;
            s.temp = 1500.0 + local_t * 3500.0; // 1500 to 5000K
            s.lum = 1.0 + local_t * 100.0;
            s.radius = 8.0 - local_t * 3.0;
        }
        else if (t <= 0.70) {
            s.stageName = "Seq. Principal Massiva (Azul)";
            s.stageIndex = 2;
            double local_t = (t - 0.15) / 0.55;
            double msTemp = (mass > 20.0) ? 38000.0 : 22000.0;
            double msLum = (mass > 20.0) ? 400000.0 : 8000.0;
            double msRad = (mass > 20.0) ? 14.0 : 5.0;
            
            s.temp = msTemp + local_t * (msTemp * 0.05);
            s.lum = msLum + local_t * (msLum * 0.2);
            s.radius = msRad + local_t * (msRad * 0.15);
        }
        else if (t <= 0.83) {
            s.stageName = (mass > 20.0) ? "Supergigante Azul/Vermelha" : "Supergigante Vermelha";
            s.stageIndex = 3;
            double local_t = (t - 0.70) / 0.13;
            double startTemp = (mass > 20.0) ? 40000.0 : 23000.0;
            double startLum = (mass > 20.0) ? 480000.0 : 9600.0;
            double startRad = (mass > 20.0) ? 16.0 : 5.8;
            
            s.temp = startTemp - local_t * (startTemp - 3300.0); // cools to 3300K
            s.lum = startLum + local_t * (mass > 20.0 ? 800000.0 : 90000.0);
            s.radius = startRad + local_t * (mass > 20.0 ? 1400.0 : 700.0); // swells massive!
        }
        else if (t <= 0.88) {
            s.stageName = "Supernova (Explosão)";
            s.stageIndex = 4;
            // Peak luminosity and extreme heat
            double local_t = (t - 0.83) / 0.05;
            s.temp = 1e7 + (1e8 - 1e7) * local_t; // millions of degrees
            s.lum = 1e9 * (1.0 - local_t) + 1e5; // extreme peak then drops
            s.radius = 50.0 + local_t * 500.0; // expanding shockwave
        }
        else {
            if (mass > 20.0) {
                s.stageName = "Buraco Negro (Remanescente)";
                s.stageIndex = 5;
                s.temp = 0.0; // Hawking temp is near 0
                s.lum = 0.0; // Completely dark
                s.radius = 0.00002; // Event horizon is extremely small
            } else {
                s.stageName = "Estrela de Nêutrons (Pulsar)";
                s.stageIndex = 5;
                double local_t = (t - 0.88) / 0.12;
                s.temp = 1000000.0 - local_t * 900000.0; // cooling core
                s.lum = 10.0 * (1.0 - local_t) + 0.01;
                s.radius = 0.0001; // extremely dense
            }
        }
    }
    
    return s;
}

// Particle generators for different stages
void SpawnNebulaParticles(float cx, float cy, float radius) {
    if (particles.size() >= 300) return;
    
    // Spawn 2 particles per frame
    for (int i = 0; i < 2; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        float r = ((float)rand() / RAND_MAX) * radius;
        float px = cx + r * cosf(angle);
        float py = cy + r * sinf(angle);
        
        Particle p;
        p.x = px;
        p.y = py;
        
        // slow collapse towards center
        p.vx = (cx - px) * 0.15f + ((float)rand() / RAND_MAX * 10.0f - 5.0f);
        p.vy = (cy - py) * 0.15f + ((float)rand() / RAND_MAX * 10.0f - 5.0f);
        
        // gaseous colors (pink, purple, cyan)
        int choice = rand() % 3;
        if (choice == 0) { p.r = 0.9f; p.g = 0.2f; p.b = 0.6f; } // Pink
        else if (choice == 1) { p.r = 0.5f; p.g = 0.1f; p.b = 0.8f; } // Purple
        else { p.r = 0.2f; p.g = 0.7f; p.b = 0.9f; } // Cyan
        
        p.a = 0.3f;
        p.size = 15.0f + (float)rand() / RAND_MAX * 15.0f;
        p.life = 3.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

void SpawnPlanetaryNebulaParticles(float cx, float cy, float innerRad) {
    // Expand outwards
    if (particles.size() >= 800) return;
    
    for (int i = 0; i < 8; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        float px = cx + innerRad * cosf(angle);
        float py = cy + innerRad * sinf(angle);
        
        Particle p;
        p.x = px;
        p.y = py;
        
        float speed = 40.0f + (float)rand() / RAND_MAX * 35.0f;
        p.vx = speed * cosf(angle);
        p.vy = speed * sinf(angle);
        
        // gas colors: neon orange, green, cyan
        int choice = rand() % 3;
        if (choice == 0) { p.r = 1.0f; p.g = 0.4f; p.b = 0.1f; } // Orange
        else if (choice == 1) { p.r = 0.2f; p.g = 0.8f; p.b = 0.3f; } // Green
        else { p.r = 0.1f; p.g = 0.7f; p.b = 0.9f; } // Cyan
        
        p.a = 0.6f;
        p.size = 6.0f + (float)rand() / RAND_MAX * 10.0f;
        p.life = 4.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

void SpawnSupernovaParticles(float cx, float cy) {
    // Extreme explosion burst
    if (particles.size() >= 1200) return;
    
    for (int i = 0; i < 30; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        
        Particle p;
        p.x = cx;
        p.y = cy;
        
        float speed = 120.0f + (float)rand() / RAND_MAX * 240.0f;
        p.vx = speed * cosf(angle);
        p.vy = speed * sinf(angle);
        
        // white, bright yellow, bright orange
        int choice = rand() % 3;
        if (choice == 0) { p.r = 1.0f; p.g = 1.0f; p.b = 1.0f; } // White
        else if (choice == 1) { p.r = 1.0f; p.g = 0.9f; p.b = 0.2f; } // Yellow
        else { p.r = 1.0f; p.g = 0.5f; p.b = 0.1f; } // Orange
        
        p.a = 0.8f;
        p.size = 8.0f + (float)rand() / RAND_MAX * 8.0f;
        p.life = 1.5f + (float)rand() / RAND_MAX * 1.5f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

void SpawnBlackHoleDiskParticles(float cx, float cy, float radius) {
    if (particles.size() >= 600) return;
    
    for (int i = 0; i < 4; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        float dist = radius + ((float)rand() / RAND_MAX) * 80.0f;
        
        Particle p;
        p.x = cx + dist * cosf(angle);
        p.y = cy + dist * sinf(angle);
        
        // Orbit speed (perpendicular to radial vector)
        float orbSpeed = 80.0f / sqrtf(dist / 30.0f);
        p.vx = -orbSpeed * sinf(angle) - (dist * 0.05f) * cosf(angle); // orbital + slight inward spiral
        p.vy = orbSpeed * cosf(angle) - (dist * 0.05f) * sinf(angle);
        
        // accretion disk glows red, orange, yellow
        int choice = rand() % 3;
        if (choice == 0) { p.r = 1.0f; p.g = 0.3f; p.b = 0.1f; }
        else if (choice == 1) { p.r = 0.9f; p.g = 0.5f; p.b = 0.1f; }
        else { p.r = 1.0f; p.g = 0.8f; p.b = 0.2f; }
        
        p.a = 0.5f;
        p.size = 2.0f + (float)rand() / RAND_MAX * 4.0f;
        p.life = 2.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

// Draw star core onion shells diagram
void DrawCoreOnionShells(float x, float y, float radius, int stageIdx, double mass) {
    // Background of core diagram
    DrawRect(x - radius - 15.0f, y - radius - 30.0f, radius * 2.0f + 30.0f, radius * 2.0f + 55.0f, 0.08f, 0.09f, 0.13f);
    DrawRectOutline(x - radius - 15.0f, y - radius - 30.0f, radius * 2.0f + 30.0f, radius * 2.0f + 55.0f, 0.2f, 0.25f, 0.35f);
    PrintString(x - radius + 5.0f, y + radius + 10.0f, "Camadas e Fusao no Núcleo", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (stageIdx == 0) { // Nebula
        PrintString(x - 55.0f, y, "Sem fusão nuclear", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        PrintString(x - 65.0f, y - 20.0f, "Colapso gravitacional", fontBaseRegular, 0.6f, 0.6f, 0.7f);
    }
    else if (stageIdx == 1) { // Protostar
        DrawFilledCircle(x, y, radius, 0.8f, 0.3f, 0.1f, 0.5f, 50);
        DrawFilledCircle(x, y, radius * 0.4f, 1.0f, 0.5f, 0.1f, 0.9f, 50);
        
        PrintString(x - 45.0f, y - 5.0f, "Contraindo", fontBaseBold, 1.0f, 1.0f, 1.0f);
        PrintString(x - 60.0f, y - 20.0f, "Aquecimento de Core", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    }
    else if (stageIdx == 2) { // Main Sequence
        // Core fuses H -> He. Two layers: Envelope (H) and Core (He)
        DrawFilledCircle(x, y, radius, 0.8f, 0.4f, 0.1f, 0.3f, 50); // envelope H
        DrawFilledCircle(x, y, radius * 0.4f, 0.9f, 0.8f, 0.1f, 0.8f, 50); // core He
        
        PrintString(x - 30.0f, y + radius - 20.0f, "Envoltório (H)", fontBaseRegular, 0.7f, 0.7f, 0.8f);
        PrintString(x - 30.0f, y - 5.0f, "Core (He)", fontBaseBold, 1.0f, 1.0f, 0.2f);
        PrintString(x - 45.0f, y - 20.0f, "Fusão: H -> He", fontBaseRegular, 0.2f, 0.9f, 0.3f);
    }
    else if (stageIdx == 3) { // Giant / Supergiant
        if (mass < 8.0) { // Red Giant
            // Three layers: H envelope, He shell (fusing H), C/O core
            DrawFilledCircle(x, y, radius, 0.8f, 0.2f, 0.1f, 0.3f, 50); // H
            DrawFilledCircle(x, y, radius * 0.6f, 0.9f, 0.7f, 0.1f, 0.6f, 50); // He
            DrawFilledCircle(x, y, radius * 0.25f, 0.2f, 0.7f, 0.8f, 0.9f, 50); // C/O core
            
            PrintString(x - 35.0f, y + radius - 20.0f, "H", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 35.0f, y + radius * 0.35f, "He (Fusao H)", fontBaseRegular, 0.9f, 0.7f, 0.2f);
            PrintString(x - 20.0f, y - 5.0f, "C + O", fontBaseBold, 0.2f, 0.8f, 0.9f);
            PrintString(x - 35.0f, y - 20.0f, "Fusão: He -> C", fontBaseRegular, 0.2f, 0.9f, 0.3f);
        } else { // Red/Blue Supergiant (Onion layers up to Iron)
            // Draw multiple concentric shell layers
            // Envelope, He, C, Ne, O, Si, Fe
            float rFactors[] = { 1.0f, 0.85f, 0.7f, 0.55f, 0.4f, 0.25f, 0.12f };
            float colors[][3] = {
                { 0.8f, 0.2f, 0.1f }, // H envelope
                { 0.9f, 0.6f, 0.1f }, // He
                { 0.8f, 0.8f, 0.1f }, // C
                { 0.2f, 0.8f, 0.3f }, // Ne
                { 0.2f, 0.6f, 0.8f }, // O
                { 0.5f, 0.3f, 0.8f }, // Si
                { 0.7f, 0.7f, 0.7f }  // Fe core
            };
            const char* labels[] = { "H", "He", "C", "Ne", "O", "Si", "Fe (Estavel)" };
            
            for (int i = 0; i < 7; ++i) {
                DrawFilledCircle(x, y, radius * rFactors[i], colors[i][0], colors[i][1], colors[i][2], 0.8f, 50);
            }
            
            // Labels pointing out
            for (int i = 0; i < 7; ++i) {
                float ly = y - radius + 15.0f + i * 14.0f;
                DrawRect(x - radius + 10.0f, ly + 2.0f, 10.0f, 6.0f, colors[i][0], colors[i][1], colors[i][2]);
                PrintString(x - radius + 25.0f, ly, labels[i], fontBaseRegular, 0.8f, 0.8f, 0.9f);
            }
            PrintString(x + 5.0f, y - 5.0f, "Core Ferro", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(x + 5.0f, y - 20.0f, "Fusão cessa", fontBaseRegular, 0.9f, 0.4f, 0.4f);
        }
    }
    else if (stageIdx == 4) { // Explosion/Nova
        PrintString(x - 55.0f, y, "Desintegração", fontBaseBold, 1.0f, 0.2f, 0.2f);
        PrintString(x - 65.0f, y - 20.0f, "Ejeção de matéria", fontBaseRegular, 0.9f, 0.5f, 0.2f);
    }
    else { // Remnant
        if (mass < 8.0) { // White dwarf (degeneracy pressure carbon/oxygen)
            DrawFilledCircle(x, y, radius * 0.3f, 0.4f, 0.8f, 1.0f, 0.9f, 50);
            PrintString(x - 50.0f, y - 5.0f, "Matéria Degenerada", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 40.0f, y - 20.0f, "Carbono e Oxigênio", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        } else if (mass <= 20.0) { // Neutron Star
            DrawFilledCircle(x, y, radius * 0.15f, 0.6f, 0.9f, 1.0f, 0.9f, 50);
            PrintString(x - 50.0f, y - 5.0f, "Nêutrons Degenerados", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 45.0f, y - 20.0f, "Extrema densidade", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        } else { // Black Hole
            DrawFilledCircle(x, y, radius * 0.1f, 0.0f, 0.0f, 0.0f, 1.0f, 50);
            PrintString(x - 30.0f, y - 5.0f, "Singularidade", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 40.0f, y - 20.0f, "Densidade infinita", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        }
    }
}

// Draw the H-R Diagram
void DrawHRDiagram(float x, float y, float w, float h, StarState current) {
    // Background box
    DrawRect(x, y, w, h, 0.07f, 0.08f, 0.12f);
    DrawRectOutline(x, y, w, h, 0.2f, 0.25f, 0.35f, 2.0f);
    
    // Label H-R Diagram
    PrintString(x + 15.0f, y + h - 22.0f, "Diagrama de Hertzsprung-Russell (H-R)", fontBaseBold, 0.9f, 0.9f, 1.0f);
    
    // Grid Lines and Labels for Y-axis (Luminosity, log10 L/L_sun, from -4 to 6)
    for (int l = -4; l <= 6; l += 2) {
        float ly = y + ((double)(l - (-4)) / 10.0) * h;
        // Horizontal line
        glColor4f(0.15f, 0.18f, 0.28f, 0.5f);
        glBegin(GL_LINES);
        glVertex2f(x, ly);
        glVertex2f(x + w, ly);
        glEnd();
        
        char lbl[16];
        if (l == 0) strcpy(lbl, "1");
        else sprintf(lbl, "10^%d", l);
        PrintString(x - 40.0f, ly - 4.0f, lbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
    }
    
    // Grid Lines and Labels for X-axis (Temperature, log10 Temp, 40,000K down to 2,000K, reversed)
    double temps[] = { 40000.0, 30000.0, 20000.0, 10000.0, 7500.0, 6000.0, 5000.0, 4000.0, 3000.0, 2000.0 };
    for (double tVal : temps) {
        // map log10(tVal)
        double minLog = log10(2000.0);
        double maxLog = log10(40000.0);
        double valLog = log10(tVal);
        float lx = x + w - ((valLog - minLog) / (maxLog - minLog)) * w;
        
        glColor4f(0.15f, 0.18f, 0.28f, 0.5f);
        glBegin(GL_LINES);
        glVertex2f(lx, y);
        glVertex2f(lx, y + h);
        glEnd();
        
        char lbl[16];
        sprintf(lbl, "%.0fK", tVal);
        // Only print some labels to avoid clutter
        if (tVal == 40000.0 || tVal == 10000.0 || tVal == 6000.0 || tVal == 3000.0 || tVal == 2000.0) {
            PrintString(lx - 15.0f, y - 16.0f, lbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
        }
    }
    
    PrintString(x + w/2 - 50.0f, y - 32.0f, "Temperatura (K) -> Fria", fontBaseBold, 0.6f, 0.6f, 0.7f);
    PrintString(x - 52.0f, y + h/2 - 10.0f, "Luminosidade (L/Sol)", fontBaseBold, 0.6f, 0.6f, 0.7f);
    
    // Draw Main Sequence Shaded Band
    glColor4f(0.18f, 0.25f, 0.40f, 0.35f);
    glBegin(GL_QUAD_STRIP);
    // Draw main sequence curve (from 40000K, 10^5 L to 2000K, 10^-4 L)
    double msTemps[] = { 40000.0, 25000.0, 15000.0, 10000.0, 7500.0, 6000.0, 5000.0, 4000.0, 3000.0, 2000.0 };
    double msLums[] =  { 1e5,     1e4,     800.0,   40.0,    5.0,     1.0,     0.25,    0.04,    4e-3,   1e-4 };
    
    double minLogT = log10(2000.0);
    double maxLogT = log10(40000.0);
    double minLogL = log10(1e-4);
    double maxLogL = log10(1e6);
    
    for (int i = 0; i < 10; ++i) {
        double tLog = log10(msTemps[i]);
        double lLogLower = log10(msLums[i] * 0.4);
        double lLogUpper = log10(msLums[i] * 2.5);
        
        float sx = x + w - ((tLog - minLogT) / (maxLogT - minLogT)) * w;
        float sy1 = y + ((lLogLower - minLogL) / (maxLogL - minLogL)) * h;
        float sy2 = y + ((lLogUpper - minLogL) / (maxLogL - minLogL)) * h;
        
        glVertex2f(sx, sy1);
        glVertex2f(sx, sy2);
    }
    glEnd();
    PrintString(x + w * 0.45f, y + h * 0.45f, "Seqüência Principal", fontBaseRegular, 0.35f, 0.45f, 0.65f);
    
    // Draw Giant / Supergiant zones
    PrintString(x + w * 0.65f, y + h * 0.78f, "Supergigantes", fontBaseRegular, 0.7f, 0.5f, 0.5f);
    PrintString(x + w * 0.75f, y + h * 0.62f, "Gigantes", fontBaseRegular, 0.7f, 0.5f, 0.5f);
    PrintString(x + w * 0.15f, y + h * 0.20f, "Anãs Brancas", fontBaseRegular, 0.5f, 0.6f, 0.7f);

    // Draw historical trail
    if (hrTrail.size() > 1) {
        glColor3f(1.0f, 0.8f, 0.2f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (const auto& pt : hrTrail) {
            if (pt.first < 2000.0) continue; // Black hole or cold remnant is off-chart
            double tLog = log10(pt.first);
            double lLog = log10(pt.second);
            
            // clamp
            if (tLog < minLogT) tLog = minLogT;
            if (tLog > maxLogT) tLog = maxLogT;
            if (lLog < minLogL) lLog = minLogL;
            if (lLog > maxLogL) lLog = maxLogL;
            
            float sx = x + w - ((tLog - minLogT) / (maxLogT - minLogT)) * w;
            float sy = y + ((lLog - minLogL) / (maxLogL - minLogL)) * h;
            glVertex2f(sx, sy);
        }
        glEnd();
    }
    
    // Draw current star position
    if (current.temp >= 2000.0) {
        double tLog = log10(current.temp);
        double lLog = log10(current.lum);
        
        // clamp
        if (tLog < minLogT) tLog = minLogT;
        if (tLog > maxLogT) tLog = maxLogT;
        if (lLog < minLogL) lLog = minLogL;
        if (lLog > maxLogL) lLog = maxLogL;
        
        float sx = x + w - ((tLog - minLogT) / (maxLogT - minLogT)) * w;
        float sy = y + ((lLog - minLogL) / (maxLogL - minLogL)) * h;
        
        // pulsing yellow dot
        static float pulse = 0.0f;
        pulse += 0.1f;
        float rad = 6.0f + 2.0f * sinf(pulse);
        
        DrawFilledCircle(sx, sy, rad, 1.0f, 0.3f, 0.1f, 1.0f, 20);
        DrawFilledCircle(sx, sy, rad - 3.0f, 1.0f, 0.9f, 0.2f, 1.0f, 20);
    }
}

// GUI helper DrawButton
bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);
    
    float r = 0.12f, g = 0.14f, b = 0.22f;
    if (active) {
        r = 0.16f; g = 0.40f; b = 0.90f;
    } else if (hovered) {
        r = 0.20f; g = 0.23f; b = 0.35f;
    }
    
    DrawRect(x, y, w, h, r, g, b);
    DrawRectOutline(x, y, w, h, 0.3f, 0.4f, 0.6f);
    
    int labelLen = strlen(label);
    float textW = labelLen * 7.0f;
    float tx = x + (w - textW) / 2.0f;
    float ty = y + (h - 10.0f) / 2.0f;
    
    PrintString(tx, ty, label, fontBaseBold, 1.0f, 1.0f, 1.0f);
    
    return hovered && clicked;
}

// Reset trail and particles on mass change
void ResetSimulation() {
    agePercent = 0.0;
    hrTrail.clear();
    particles.clear();
}

// Render main visualizer panel
void DrawStellarVisualizer(float cx, float cy, StarState s) {
    // Star visual drawing centered at cx, cy
    if (s.stageIndex == 0) { // Nebula
        SpawnNebulaParticles(cx, cy, 200.0f);
        DrawParticles();
        PrintString(cx - 50.0f, cy, "Nébula de Gás", fontBaseBold, 0.8f, 0.5f, 0.9f);
    }
    else if (s.stageIndex == 1) { // Protostar
        SpawnNebulaParticles(cx, cy, 120.0f);
        DrawParticles();
        // Protostar center glow
        DrawFilledCircle(cx, cy, 30.0f, 1.0f, 0.3f, 0.1f, 0.3f, 50);
        DrawFilledCircle(cx, cy, 20.0f, 1.0f, 0.5f, 0.1f, 0.6f, 50);
        DrawFilledCircle(cx, cy, 10.0f, 1.0f, 0.8f, 0.2f, 0.9f, 50);
    }
    else if (s.stageIndex == 2) { // Main sequence
        // Stable glowing star
        float visualR = 40.0f + float(s.radius * 3.0);
        if (visualR > 120.0f) visualR = 120.0f;
        
        float rColor = 1.0f, gColor = 0.9f, bColor = 0.2f; // Sun yellow
        if (initialMass < 0.8) { rColor = 0.9f; gColor = 0.2f; bColor = 0.1f; } // Red dwarf
        else if (initialMass > 8.0) { rColor = 0.2f; gColor = 0.6f; bColor = 1.0f; } // Blue giant
        
        // draw outer glow layers
        for (int i = 5; i > 0; --i) {
            float alpha = 0.1f * i;
            DrawFilledCircle(cx, cy, visualR * (1.0f + 0.15f * (6 - i)), rColor, gColor, bColor, alpha, 60);
        }
        DrawFilledCircle(cx, cy, visualR, rColor, gColor, bColor, 1.0f, 60);
        DrawFilledCircle(cx, cy, visualR * 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 60); // bright core
    }
    else if (s.stageIndex == 3) { // Giant / Supergiant
        // Swelled star, deep red-orange
        float visualR = 100.0f + float(s.radius * 0.05); // scale down giant to fit screen
        if (visualR > 180.0f) visualR = 180.0f;
        if (visualR < 80.0f) visualR = 80.0f;
        
        float rColor = 0.9f, gColor = 0.2f, bColor = 0.1f; // deep red
        if (initialMass > 20.0 && agePercent < 0.76) {
            rColor = 0.2f; gColor = 0.5f; bColor = 0.9f; // Blue supergiant stage
        }

        for (int i = 5; i > 0; --i) {
            float alpha = 0.08f * i;
            DrawFilledCircle(cx, cy, visualR * (1.0f + 0.12f * (6 - i)), rColor, gColor, bColor, alpha, 60);
        }
        DrawFilledCircle(cx, cy, visualR, rColor, gColor, bColor, 1.0f, 60);
        DrawFilledCircle(cx, cy, visualR * 0.5f, 1.0f, 0.6f, 0.1f, 1.0f, 60);
    } else if (s.stageIndex == 4) { // Transition / Explosion
        if (initialMass < 8.0) { // Planetary Nebula
            SpawnPlanetaryNebulaParticles(cx, cy, 35.0f);
            DrawParticles();
            // Tiny emerging white dwarf in center
            DrawFilledCircle(cx, cy, 6.0f, 0.4f, 0.8f, 1.0f, 1.0f, 40);
        } else { // Supernova
            SpawnSupernovaParticles(cx, cy);
            DrawParticles();
            // Flash effect
            static float flashAlpha = 1.0f;
            if (flashAlpha > 0.0f) {
                DrawRect(25, 25, 730, 750, 1.0f, 1.0f, 1.0f, flashAlpha);
                flashAlpha -= 0.04f;
            } else {
                if (rand() % 5 == 0) flashAlpha = 0.15f; // micro flashes
            }
        }
    } else { // Remnants
        if (initialMass < 8.0) { // White Dwarf
            // Small bright blue-white dot with soft halo
            DrawFilledCircle(cx, cy, 35.0f, 0.3f, 0.7f, 1.0f, 0.1f, 40);
            DrawFilledCircle(cx, cy, 15.0f, 0.3f, 0.7f, 1.0f, 0.3f, 40);
            DrawFilledCircle(cx, cy, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f, 40);
        } else if (initialMass <= 20.0) { // Neutron Star / Pulsar
            // Tiny blue core
            DrawFilledCircle(cx, cy, 12.0f, 0.5f, 0.8f, 1.0f, 0.4f, 30);
            DrawFilledCircle(cx, cy, 4.0f, 1.0f, 1.0f, 1.0f, 1.0f, 30);

            // Rotating beams (electric cones)
            static float beamAngle = 0.0f;
            beamAngle += 0.15f;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(0.3f, 0.7f, 1.0f, 0.5f);

            // Beam 1
            glBegin(GL_TRIANGLES);
            glVertex2f(cx, cy);
            glVertex2f(cx + 250.0f * cosf(beamAngle - 0.12f), cy + 250.0f * sinf(beamAngle - 0.12f));
            glVertex2f(cx + 250.0f * cosf(beamAngle + 0.12f), cy + 250.0f * sinf(beamAngle + 0.12f));
            glEnd();

            // Beam 2 (opposite direction)
            float oppAngle = beamAngle + PI_F;
            glBegin(GL_TRIANGLES);
            glVertex2f(cx, cy);
            glVertex2f(cx + 250.0f * cosf(oppAngle - 0.12f), cy + 250.0f * sinf(oppAngle - 0.12f));
            glVertex2f(cx + 250.0f * cosf(oppAngle + 0.12f), cy + 250.0f * sinf(oppAngle + 0.12f));
            glEnd();
            glDisable(GL_BLEND);
        } else { // Black hole
            // Accretion disk
            SpawnBlackHoleDiskParticles(cx, cy, 45.0f);
            DrawParticles();

            // Event Horizon
            DrawFilledCircle(cx, cy, 25.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50);

            // Gravitational lensing aura
            glLineWidth(2.0f);
            DrawCircle(cx, cy, 28.0f, 0.9f, 0.5f, 0.1f, 60);
        }
    }
}

// Scene Drawing
void DrawGLScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(g_hwnd, &mousePos);
    int mx = mousePos.x;
    int my = 800 - mousePos.y;
    static bool lastMouseState = false;
    bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool clicked = currentMouseState && !lastMouseState;
    lastMouseState = currentMouseState;

    glViewport(0, 0, 1200, 800);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1200, 0, 800, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Background
    DrawRect(0, 0, 1200, 800, 0.05f, 0.05f, 0.08f);

    // Panels
    DrawRect(15, 15, 750, 770, 0.09f, 0.10f, 0.15f); // Left Panel
    DrawRectOutline(15, 15, 750, 770, 0.18f, 0.22f, 0.35f, 2.0f);

    DrawRect(780, 15, 405, 770, 0.09f, 0.10f, 0.15f); // Right Panel
    DrawRectOutline(780, 15, 405, 770, 0.18f, 0.22f, 0.35f, 2.0f);

    // Title
    PrintString(800, 750, "CICLO DE VIDA ESTELAR", fontBaseLarge, 1.0f, 1.0f, 1.0f);
    PrintString(800, 730, "Evolucao estelar baseada na massa", fontBaseRegular, 0.6f, 0.6f, 0.7f);

    // Sidebar mass selection buttons
    float massY = 510.0f;
    PrintString(800, massY + 30, "Massa Estelar Inicial:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (DrawButton(800, massY, 80, 30, "0.5 M☉", initialMass == 0.5, mx, my, clicked)) {
        initialMass = 0.5; ResetSimulation();
    }
    if (DrawButton(890, massY, 80, 30, "1.0 M☉", initialMass == 1.0, mx, my, clicked)) {
        initialMass = 1.0; ResetSimulation();
    }
    if (DrawButton(980, massY, 80, 30, "10 M☉", initialMass == 10.0, mx, my, clicked)) {
        initialMass = 10.0; ResetSimulation();
    }
    if (DrawButton(1070, massY, 80, 30, "30 M☉", initialMass == 30.0, mx, my, clicked)) {
        initialMass = 30.0; ResetSimulation();
    }

    // Sidebar: Simulation Controls
    float ctrlY = 410.0f;
    PrintString(800, ctrlY + 30, "Controles da Simulacao:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    if (DrawButton(800, ctrlY, 110, 30, isRunning ? "Pausar" : "Iniciar", false, mx, my, clicked)) {
        isRunning = !isRunning;
    }
    if (DrawButton(920, ctrlY, 110, 30, "Avancar 5%", false, mx, my, clicked)) {
        agePercent += 0.05;
        if (agePercent > 1.0) agePercent = 0.0;
    }
    if (DrawButton(1040, ctrlY, 125, 30, "Reiniciar", false, mx, my, clicked)) {
        ResetSimulation();
    }

    // Aging Speed Slider
    float speedY = 350.0f;
    PrintString(800, speedY + 20, "Velocidade de Evolucao:", fontBaseBold, 0.8f, 0.8f, 0.9f);

    if (DrawButton(800, speedY, 80, 24, "Lento", simSpeed == 0.002, mx, my, clicked)) simSpeed = 0.002;
    if (DrawButton(890, speedY, 80, 24, "Medio", simSpeed == 0.01, mx, my, clicked)) simSpeed = 0.01;
    if (DrawButton(980, speedY, 80, 24, "Rapido", simSpeed == 0.04, mx, my, clicked)) simSpeed = 0.04;

    // Calculate current properties
    StarState s = GetStellarProperties(initialMass, agePercent);

    // Store current state in history trail
    if (hrTrail.empty() || hrTrail.back().second != s.lum || hrTrail.back().first != s.temp) {
        hrTrail.push_back({s.temp, s.lum});
    }

    // Readout panel (Right panel bottom)
    float statsY = 270.0f;
    PrintString(800, statsY + 30, "Dados Físicos Atuais:", fontBaseBold, 0.8f, 0.8f, 0.9f);

    char nameBuf[64], ageBuf[64], tempBuf[64], radBuf[64], lumBuf[64];
    sprintf(nameBuf, "Estágio:  %s", s.stageName.c_str());

    if (s.ageYears > 1000.0) {
        sprintf(ageBuf, "Idade:   %.2f Bilhoes de anos", s.ageYears / 1000.0);
    } else {
        sprintf(ageBuf, "Idade:   %.1f Milhoes de anos", s.ageYears);
    }

    if (s.temp == 0.0) sprintf(tempBuf, "Temp: Singularidade (0K)");
    else sprintf(tempBuf, "Temp:%.0f K", s.temp);

    if (s.radius < 0.01) sprintf(radBuf, "Raio: %.4f R☉ (Estrela compacta)", s.radius);
    else sprintf(radBuf, "Raio: %.2f R☉", s.radius);

    if (s.lum == 0.0) sprintf(lumBuf, "Brilho: Totalmente Negro (0 L☉)");
    else sprintf(lumBuf, "Brilho: %.2f L☉", s.lum);

    PrintString(800, statsY, nameBuf, fontBaseBold, 1.0f, 0.8f, 0.2f);
    PrintString(800, statsY - 20, ageBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 40, tempBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 60, radBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 80, lumBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);

    // Left Panel Drawings:
    // Visualizer takes the top half of left panel, Core diagram takes bottom-left, H-R diagram takes bottom-right
    // Wait, let's design the layout inside the left panel (750x770 area: x=15 to 765, y=15 to 785):
    // Main Visualizer: x=30 to 750, y=410 to 770 (height 360)
    // Core burning shells: x=30 to 330, y=30 to 380 (width 300, height 350)
    // H-R Diagram: x=360 to 750, y=30 to 380 (width 390, height 350)

    // Outline areas inside Left Panel for clean UI structure
    DrawRectOutline(30, 410, 720, 310, 0.15f, 0.18f, 0.28f);
    PrintString(45, 695, "Visualizacao da Estrela", fontBaseBold, 0.9f, 0.9f, 1.0f);

    // Main Visualizer draw
    DrawStellarVisualizer(390.0f, 560.0f, s);

    // Draw Core onion diagram
    DrawCoreOnionShells(180.0f, 200.0f, 100.0f, s.stageIndex, initialMass);

    // Draw H-R Diagram
    DrawHRDiagram(380.0f, 60.0f, 350.0f, 310.0f, s);

    SwapBuffers(g_hdc);
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    if (!CreateGLWindow("Simulacao de Ciclo de Vida das Estrelas", 1200, 800)) {
        printf("Falha ao criar janela OpenGL.\n");
        return 1;
    }

    BuildFonts();
    ResetSimulation();

    auto lastTick = std::chrono::high_resolution_clock::now();

    while (true) {
        ProcessMessages();
        
        auto currentTick = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = currentTick - lastTick;
        lastTick = currentTick;
        float dt = elapsed.count();
        if (dt > 0.1f) dt = 0.1f; // limit large steps

        if (isRunning) {
            agePercent += simSpeed * dt;
            if (agePercent > 1.0) {
                // Loop simulation back to Nebula stage
                agePercent = 0.0;
                hrTrail.clear();
                particles.clear();
            }
        }

        UpdateParticles(dt);
        DrawGLScene();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
