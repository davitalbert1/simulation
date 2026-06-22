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

const float PI_F = 3.14159265f;

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
        particles[i].a = (particles[i].life / particles[i].maxLife) * 0.7f;
        
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
    for (const auto& p : particles) {
        DrawFilledCircle(p.x, p.y, p.size, p.r, p.g, p.b, p.a, 8);
    }
    glDisable(GL_BLEND);
}

double initialMass = 1.0;
double agePercent = 0.0;
bool isRunning = true;
double simSpeed = 0.01;

struct StarState {
    std::string stageName;
    int stageIndex;
    double temp;
    double lum;
    double radius;
    double ageYears;
};

std::vector<std::pair<double, double>> hrTrail;

// Função de evolução estelar: Interpola propriedades físicas (Temperatura, Luminosidade, Raio, Idade) com base na massa inicial e no avanço do ciclo de vida da estrela.
StarState GetStellarProperties(double mass, double agePct) {
    StarState s;
    s.ageYears = 0.0;
    
    double lifetime = 10000.0;
    if (mass < 0.8) lifetime = 50000.0;
    else if (mass > 25.0) lifetime = 5.0;
    else if (mass > 8.0) lifetime = 30.0;
    
    double t = agePct;
    
    if (mass < 8.0) {
        s.ageYears = agePct * lifetime;
        
        if (t <= 0.08) {
            // Estágio de Nebulosa Estelar: Grande nuvem interestelar de gás e poeira que colapsa gravitacionalmente sob o seu próprio peso.
            s.stageName = "Nébula Estelar (Berço)";
            s.stageIndex = 0;
            double local_t = t / 0.08;
            s.temp = 50.0 + local_t * 950.0;
            s.lum = 1e-4 + local_t * 1e-2;
            s.radius = 10.0 - local_t * 8.0;
        }
        else if (t <= 0.15) {
            // Estágio de Protoestrela: Fase de contração adiabática onde a energia potencial gravitacional é liberada na forma de calor, brilhando no infravermelho.
            s.stageName = "Protoestrela";
            s.stageIndex = 1;
            double local_t = (t - 0.08) / 0.07;
            s.temp = 1000.0 + local_t * 2000.0;
            s.lum = 1e-2 + local_t * 5.0;
            s.radius = 2.0 - local_t * 1.0;
        }
        else if (t <= 0.70) {
            // Estágio de Seqüência Principal: Longo período de equilíbrio hidrostático, onde a contração da gravidade é balanceada pela fusão nuclear de Hidrogênio em Hélio no núcleo (Cadeia Próton-Próton).
            s.stageName = "Seqüência Principal";
            s.stageIndex = 2;
            double local_t = (t - 0.15) / 0.55;
            double msTemp = (mass < 0.8) ? 3800.0 : 5800.0;
            double msLum = (mass < 0.8) ? 0.08 : 1.0;
            double msRad = (mass < 0.8) ? 0.6 : 1.0;
            
            s.temp = msTemp + local_t * (msTemp * 0.1);
            s.lum = msLum + local_t * (msLum * 0.3);
            s.radius = msRad + local_t * (msRad * 0.1);
        }
        else if (t <= 0.83) {
            // Estágio de Gigante Vermelha: O Hidrogênio do núcleo se esgota, o núcleo contrai e aquece para fundir Hélio em Carbono (Processo Triplo-Alfa), enquanto o envoltório se expande e resfria.
            s.stageName = "Gigante Vermelha";
            s.stageIndex = 3;
            double local_t = (t - 0.70) / 0.13;
            double startTemp = (mass < 0.8) ? 4100.0 : 6200.0;
            double startLum = (mass < 0.8) ? 0.1 : 1.3;
            double startRad = (mass < 0.8) ? 0.65 : 1.1;
            
            s.temp = startTemp - local_t * (startTemp - 3100.0);
            s.lum = startLum + local_t * 1500.0;
            s.radius = startRad + local_t * 120.0;
        }
        else if (t <= 0.88) {
            // Estágio de Nebulosa Planetária: As pulsações térmicas causam a ejeção das camadas externas da estrela no meio interestelar, restando apenas o núcleo exposto.
            s.stageName = "Nebulosa Planetária";
            s.stageIndex = 4;
            double local_t = (t - 0.83) / 0.05;
            s.temp = 3100.0 + local_t * 96900.0;
            s.lum = 1500.0 - local_t * 1400.0;
            s.radius = 120.0 - local_t * 119.5;
        }
        else {
            // Anã Branca: Núcleo denso e inerte de Carbono e Oxigênio sustentado contra a gravidade pela pressão de degeneração eletrônica (Princípio de Exclusão de Pauli) que apenas esfria progressivamente.
            s.stageName = "Anã Branca (Remanescente)";
            s.stageIndex = 5;
            double local_t = (t - 0.88) / 0.12;
            s.temp = 100000.0 - local_t * 95000.0;
            s.lum = 100.0 * pow(1.0 - local_t, 3) + 1e-4;
            s.radius = 0.01;
        }
    }
    else {
        s.ageYears = agePct * lifetime;
        
        if (t <= 0.08) {
            // Estágio de Nebulosa Estelar Massiva.
            s.stageName = "Nébula Estelar (Berço)";
            s.stageIndex = 0;
            double local_t = t / 0.08;
            s.temp = 50.0 + local_t * 1450.0;
            s.lum = 1e-3 + local_t * 1.0;
            s.radius = 20.0 - local_t * 12.0;
        }
        else if (t <= 0.15) {
            // Estágio de Protoestrela Massiva.
            s.stageName = "Protoestrela Massiva";
            s.stageIndex = 1;
            double local_t = (t - 0.08) / 0.07;
            s.temp = 1500.0 + local_t * 3500.0;
            s.lum = 1.0 + local_t * 100.0;
            s.radius = 8.0 - local_t * 3.0;
        }
        else if (t <= 0.70) {
            // Seqüência Principal Massiva: Fusão acelerada de Hidrogênio em Hélio através do Ciclo CNO devido às altíssimas pressões e temperaturas centrais. Estrela azul extremamente quente.
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
            // Supergigante: Estrela de alta massa que queima sucessivos elementos pesados no núcleo (Processo Triplo-Alfa, queima de Carbono, Neônio, Oxigênio, Silício) até atingir o Ferro.
            s.stageName = (mass > 20.0) ? "Supergigante Azul/Vermelha" : "Supergigante Vermelha";
            s.stageIndex = 3;
            double local_t = (t - 0.70) / 0.13;
            double startTemp = (mass > 20.0) ? 40000.0 : 23000.0;
            double startLum = (mass > 20.0) ? 480000.0 : 9600.0;
            double startRad = (mass > 20.0) ? 16.0 : 5.8;
            
            s.temp = startTemp - local_t * (startTemp - 3300.0);
            s.lum = startLum + local_t * (mass > 20.0 ? 800000.0 : 90000.0);
            s.radius = startRad + local_t * (mass > 20.0 ? 1400.0 : 700.0);
        }
        else if (t <= 0.88) {
            // Explosão de Supernova: Colapso catastrófico do núcleo de Ferro quando este ultrapassa o Limite de Chandrasekhar. Ocorre violenta ejeção de matéria e nucleossíntese explosiva.
            s.stageName = "Supernova (Explosão)";
            s.stageIndex = 4;
            double local_t = (t - 0.83) / 0.05;
            s.temp = 1e7 + (1e8 - 1e7) * local_t;
            s.lum = 1e9 * (1.0 - local_t) + 1e5;
            s.radius = 50.0 + local_t * 500.0;
        }
        else {
            if (mass > 20.0) {
                // Buraco Negro: Objeto supermassivo cujo colapso gravitacional supera a pressão de degeneração de nêutrons, concentrando a massa em uma singularidade física além de um horizonte de eventos.
                s.stageName = "Buraco Negro (Remanescente)";
                s.stageIndex = 5;
                s.temp = 0.0;
                s.lum = 0.0;
                s.radius = 0.00002;
            } else {
                // Estrela de Nêutrons (Pulsar): Remanescente estelar denso composto quase inteiramente por nêutrons degenerados, sustentado pela pressão de degeneração neutrônica.
                s.stageName = "Estrela de Nêutrons (Pulsar)";
                s.stageIndex = 5;
                double local_t = (t - 0.88) / 0.12;
                s.temp = 1000000.0 - local_t * 900000.0;
                s.lum = 10.0 * (1.0 - local_t) + 0.01;
                s.radius = 0.0001;
            }
        }
    }
    
    return s;
}

void SpawnNebulaParticles(float cx, float cy, float radius) {
    if (particles.size() >= 300) return;
    
    for (int i = 0; i < 2; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        float r = ((float)rand() / RAND_MAX) * radius;
        float px = cx + r * cosf(angle);
        float py = cy + r * sinf(angle);
        
        Particle p;
        p.x = px;
        p.y = py;
        
        p.vx = (cx - px) * 0.15f + ((float)rand() / RAND_MAX * 10.0f - 5.0f);
        p.vy = (cy - py) * 0.15f + ((float)rand() / RAND_MAX * 10.0f - 5.0f);
        
        int choice = rand() % 3;
        if (choice == 0) {
            p.r = 0.9f;
            p.g = 0.2f;
            p.b = 0.6f;
        } else if (choice == 1) {
            p.r = 0.5f;
            p.g = 0.1f;
            p.b = 0.8f;
        } else { p.r = 0.2f;
            p.g = 0.7f;
            p.b = 0.9f;
        }
        
        p.a = 0.3f;
        p.size = 15.0f + (float)rand() / RAND_MAX * 15.0f;
        p.life = 3.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

void SpawnPlanetaryNebulaParticles(float cx, float cy, float innerRad) {
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
        
        int choice = rand() % 3;
        if (choice == 0) {
            p.r = 1.0f;
            p.g = 0.4f;
            p.b = 0.1f;
        } else if (choice == 1) {
            p.r = 0.2f;
            p.g = 0.8f;
            p.b = 0.3f;
        } else {
            p.r = 0.1f;
            p.g = 0.7f;
            p.b = 0.9f;
        }
        
        p.a = 0.6f;
        p.size = 6.0f + (float)rand() / RAND_MAX * 10.0f;
        p.life = 4.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

void SpawnSupernovaParticles(float cx, float cy) {
    if (particles.size() >= 1200) return;
    
    for (int i = 0; i < 30; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * PI_F;
        
        Particle p;
        p.x = cx;
        p.y = cy;
        
        float speed = 120.0f + (float)rand() / RAND_MAX * 240.0f;
        p.vx = speed * cosf(angle);
        p.vy = speed * sinf(angle);
        
        int choice = rand() % 3;
        if (choice == 0) { p.r = 1.0f;
            p.g = 1.0f;
            p.b = 1.0f;
        } else if (choice == 1) { p.r = 1.0f;
            p.g = 0.9f;
            p.b = 0.2f;
        } else { p.r = 1.0f;
            p.g = 0.5f;
            p.b = 0.1f;
        }
        
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
        
        float orbSpeed = 80.0f / sqrtf(dist / 30.0f);
        p.vx = -orbSpeed * sinf(angle) - (dist * 0.05f) * cosf(angle);
        p.vy = orbSpeed * cosf(angle) - (dist * 0.05f) * sinf(angle);
        
        int choice = rand() % 3;
        if (choice == 0) {
            p.r = 1.0f;
            p.g = 0.3f;
            p.b = 0.1f;
        } else if (choice == 1) {
            p.r = 0.9f;
            p.g = 0.5f;
            p.b = 0.1f;
        } else { p.r = 1.0f;
            p.g = 0.8f;
            p.b = 0.2f;
        }
        
        p.a = 0.5f;
        p.size = 2.0f + (float)rand() / RAND_MAX * 4.0f;
        p.life = 2.0f + (float)rand() / RAND_MAX * 2.0f;
        p.maxLife = p.life;
        particles.push_back(p);
    }
}

// Representação de camadas estelares: Desenha as cascas concêntricas de queima de elementos conforme o estágio evolutivo (fusão de Hidrogênio, Hélio, Carbono etc. até o núcleo inerte de Ferro em estrelas massivas).
void DrawCoreOnionShells(float x, float y, float radius, int stageIdx, double mass) {
    DrawRect(x - radius - 15.0f, y - radius - 30.0f, radius * 2.0f + 30.0f, radius * 2.0f + 55.0f, 0.08f, 0.09f, 0.13f);
    DrawRectOutline(x - radius - 15.0f, y - radius - 30.0f, radius * 2.0f + 30.0f, radius * 2.0f + 55.0f, 0.2f, 0.25f, 0.35f);
    PrintString(x - radius + 5.0f, y + radius + 10.0f, "Camadas e Fusao no Núcleo", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (stageIdx == 0) {
        PrintString(x - 55.0f, y, "Sem fusão nuclear", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        PrintString(x - 65.0f, y - 20.0f, "Colapso gravitacional", fontBaseRegular, 0.6f, 0.6f, 0.7f);
    }
    else if (stageIdx == 1) {
        DrawFilledCircle(x, y, radius, 0.8f, 0.3f, 0.1f, 0.5f, 50);
        DrawFilledCircle(x, y, radius * 0.4f, 1.0f, 0.5f, 0.1f, 0.9f, 50);
        
        PrintString(x - 45.0f, y - 5.0f, "Contraindo", fontBaseBold, 1.0f, 1.0f, 1.0f);
        PrintString(x - 60.0f, y - 20.0f, "Aquecimento de Core", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    }
    else if (stageIdx == 2) {
        DrawFilledCircle(x, y, radius, 0.8f, 0.4f, 0.1f, 0.3f, 50);
        DrawFilledCircle(x, y, radius * 0.4f, 0.9f, 0.8f, 0.1f, 0.8f, 50);
        
        PrintString(x - 30.0f, y + radius - 20.0f, "Envoltório (H)", fontBaseRegular, 0.7f, 0.7f, 0.8f);
        PrintString(x - 30.0f, y - 5.0f, "Core (He)", fontBaseBold, 1.0f, 1.0f, 0.2f);
        PrintString(x - 45.0f, y - 20.0f, "Fusão: H -> He", fontBaseRegular, 0.2f, 0.9f, 0.3f);
    }
    else if (stageIdx == 3) {
        if (mass < 8.0) {
            DrawFilledCircle(x, y, radius, 0.8f, 0.2f, 0.1f, 0.3f, 50);
            DrawFilledCircle(x, y, radius * 0.6f, 0.9f, 0.7f, 0.1f, 0.6f, 50);
            DrawFilledCircle(x, y, radius * 0.25f, 0.2f, 0.7f, 0.8f, 0.9f, 50);
            
            PrintString(x - 35.0f, y + radius - 20.0f, "H", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 35.0f, y + radius * 0.35f, "He (Fusao H)", fontBaseRegular, 0.9f, 0.7f, 0.2f);
            PrintString(x - 20.0f, y - 5.0f, "C + O", fontBaseBold, 0.2f, 0.8f, 0.9f);
            PrintString(x - 35.0f, y - 20.0f, "Fusão: He -> C", fontBaseRegular, 0.2f, 0.9f, 0.3f);
        } else {
            float rFactors[] = { 1.0f, 0.85f, 0.7f, 0.55f, 0.4f, 0.25f, 0.12f };
            float colors[][3] = {
                { 0.8f, 0.2f, 0.1f },
                { 0.9f, 0.6f, 0.1f },
                { 0.8f, 0.8f, 0.1f },
                { 0.2f, 0.8f, 0.3f },
                { 0.2f, 0.6f, 0.8f },
                { 0.5f, 0.3f, 0.8f },
                { 0.7f, 0.7f, 0.7f }
            };
            const char* labels[] = { "H", "He", "C", "Ne", "O", "Si", "Fe (Estavel)" };
            
            for (int i = 0; i < 7; ++i) {
                DrawFilledCircle(x, y, radius * rFactors[i], colors[i][0], colors[i][1], colors[i][2], 0.8f, 50);
            }
            
            for (int i = 0; i < 7; ++i) {
                float ly = y - radius + 15.0f + i * 14.0f;
                DrawRect(x - radius + 10.0f, ly + 2.0f, 10.0f, 6.0f, colors[i][0], colors[i][1], colors[i][2]);
                PrintString(x - radius + 25.0f, ly, labels[i], fontBaseRegular, 0.8f, 0.8f, 0.9f);
            }
            PrintString(x + 5.0f, y - 5.0f, "Core Ferro", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(x + 5.0f, y - 20.0f, "Fusão cessa", fontBaseRegular, 0.9f, 0.4f, 0.4f);
        }
    }
    else if (stageIdx == 4) {
        PrintString(x - 55.0f, y, "Desintegração", fontBaseBold, 1.0f, 0.2f, 0.2f);
        PrintString(x - 65.0f, y - 20.0f, "Ejeção de matéria", fontBaseRegular, 0.9f, 0.5f, 0.2f);
    }
    else {
        if (mass < 8.0) {
            DrawFilledCircle(x, y, radius * 0.3f, 0.4f, 0.8f, 1.0f, 0.9f, 50);
            PrintString(x - 50.0f, y - 5.0f, "Matéria Degenerada", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 40.0f, y - 20.0f, "Carbono e Oxigênio", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        } else if (mass <= 20.0) {
            DrawFilledCircle(x, y, radius * 0.15f, 0.6f, 0.9f, 1.0f, 0.9f, 50);
            PrintString(x - 50.0f, y - 5.0f, "Nêutrons Degenerados", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 45.0f, y - 20.0f, "Extrema densidade", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        } else {
            DrawFilledCircle(x, y, radius * 0.1f, 0.0f, 0.0f, 0.0f, 1.0f, 50);
            PrintString(x - 30.0f, y - 5.0f, "Singularidade", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(x - 40.0f, y - 20.0f, "Densidade infinita", fontBaseRegular, 0.6f, 0.6f, 0.7f);
        }
    }
}

// Diagrama de Hertzsprung-Russell (H-R): Mapeia graficamente a temperatura efetiva da estrela (eixo X invertido) contra a sua luminosidade (eixo Y logarítmico) indicando as regiões evolutivas principais.
void DrawHRDiagram(float x, float y, float w, float h, StarState current) {
    DrawRect(x, y, w, h, 0.07f, 0.08f, 0.12f);
    DrawRectOutline(x, y, w, h, 0.2f, 0.25f, 0.35f, 2.0f);
    
    PrintString(x + 15.0f, y + h - 22.0f, "Diagrama de Hertzsprung-Russell (H-R)", fontBaseBold, 0.9f, 0.9f, 1.0f);
    
    for (int l = -4; l <= 6; l += 2) {
        float ly = y + ((double)(l - (-4)) / 10.0) * h;
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
    
    double temps[] = { 40000.0, 30000.0, 20000.0, 10000.0, 7500.0, 6000.0, 5000.0, 4000.0, 3000.0, 2000.0 };
    for (double tVal : temps) {
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
        if (tVal == 40000.0 || tVal == 10000.0 || tVal == 6000.0 || tVal == 3000.0 || tVal == 2000.0) {
            PrintString(lx - 15.0f, y - 16.0f, lbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
        }
    }
    
    PrintString(x + w/2 - 50.0f, y - 32.0f, "Temperatura (K) -> Fria", fontBaseBold, 0.6f, 0.6f, 0.7f);
    PrintString(x - 52.0f, y + h/2 - 10.0f, "Luminosidade (L/Sol)", fontBaseBold, 0.6f, 0.6f, 0.7f);
    
    glColor4f(0.18f, 0.25f, 0.40f, 0.35f);
    glBegin(GL_QUAD_STRIP);
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
    
    PrintString(x + w * 0.65f, y + h * 0.78f, "Supergigantes", fontBaseRegular, 0.7f, 0.5f, 0.5f);
    PrintString(x + w * 0.75f, y + h * 0.62f, "Gigantes", fontBaseRegular, 0.7f, 0.5f, 0.5f);
    PrintString(x + w * 0.15f, y + h * 0.20f, "Anãs Brancas", fontBaseRegular, 0.5f, 0.6f, 0.7f);

    if (hrTrail.size() > 1) {
        glColor3f(1.0f, 0.8f, 0.2f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (const auto& pt : hrTrail) {
            if (pt.first < 2000.0) continue;
            double tLog = log10(pt.first);
            double lLog = log10(pt.second);
            
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
    
    if (current.temp >= 2000.0) {
        double tLog = log10(current.temp);
        double lLog = log10(current.lum);
        
        if (tLog < minLogT) tLog = minLogT;
        if (tLog > maxLogT) tLog = maxLogT;
        if (lLog < minLogL) lLog = minLogL;
        if (lLog > maxLogL) lLog = maxLogL;
        
        float sx = x + w - ((tLog - minLogT) / (maxLogT - minLogT)) * w;
        float sy = y + ((lLog - minLogL) / (maxLogL - minLogL)) * h;
        
        static float pulse = 0.0f;
        pulse += 0.1f;
        float rad = 6.0f + 2.0f * sinf(pulse);
        
        DrawFilledCircle(sx, sy, rad, 1.0f, 0.3f, 0.1f, 1.0f, 20);
        DrawFilledCircle(sx, sy, rad - 3.0f, 1.0f, 0.9f, 0.2f, 1.0f, 20);
    }
}

bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);
    
    float r = 0.12f, g = 0.14f, b = 0.22f;
    if (active) {
        r = 0.16f;
        g = 0.40f;
        b = 0.90f;
    } else if (hovered) {
        r = 0.20f;
        g = 0.23f;
        b = 0.35f;
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

void ResetSimulation() {
    agePercent = 0.0;
    hrTrail.clear();
    particles.clear();
}

void DrawStellarVisualizer(float cx, float cy, StarState s) {
    if (s.stageIndex == 0) {
        SpawnNebulaParticles(cx, cy, 200.0f);
        DrawParticles();
        PrintString(cx - 50.0f, cy, "Nébula de Gás", fontBaseBold, 0.8f, 0.5f, 0.9f);
    } else if (s.stageIndex == 1) {
        SpawnNebulaParticles(cx, cy, 120.0f);
        DrawParticles();
        DrawFilledCircle(cx, cy, 30.0f, 1.0f, 0.3f, 0.1f, 0.3f, 50);
        DrawFilledCircle(cx, cy, 20.0f, 1.0f, 0.5f, 0.1f, 0.6f, 50);
        DrawFilledCircle(cx, cy, 10.0f, 1.0f, 0.8f, 0.2f, 0.9f, 50);
    } else if (s.stageIndex == 2) {
        float visualR = 40.0f + float(s.radius * 3.0);
        if (visualR > 120.0f) visualR = 120.0f;
        
        float rColor = 1.0f, gColor = 0.9f, bColor = 0.2f;
        if (initialMass < 0.8) {
            rColor = 0.9f;
            gColor = 0.2f;
            bColor = 0.1f;
        } else if (initialMass > 8.0) {
            rColor = 0.2f;
            gColor = 0.6f;
            bColor = 1.0f;
        }
        
        for (int i = 5; i > 0; --i) {
            float alpha = 0.1f * i;
            DrawFilledCircle(cx, cy, visualR * (1.0f + 0.15f * (6 - i)), rColor, gColor, bColor, alpha, 60);
        }
        DrawFilledCircle(cx, cy, visualR, rColor, gColor, bColor, 1.0f, 60);
        DrawFilledCircle(cx, cy, visualR * 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 60);
    } else if (s.stageIndex == 3) {
        float visualR = 100.0f + float(s.radius * 0.05);
        if (visualR > 180.0f) visualR = 180.0f;
        if (visualR < 80.0f) visualR = 80.0f;
        
        float rColor = 0.9f, gColor = 0.2f, bColor = 0.1f;
        if (initialMass > 20.0 && agePercent < 0.76) {
            rColor = 0.2f;
            gColor = 0.5f;
            bColor = 0.9f;
        }
        
        for (int i = 5; i > 0; --i) {
            float alpha = 0.08f * i;
            DrawFilledCircle(cx, cy, visualR * (1.0f + 0.12f * (6 - i)), rColor, gColor, bColor, alpha, 60);
        }
        DrawFilledCircle(cx, cy, visualR, rColor, gColor, bColor, 1.0f, 60);
        DrawFilledCircle(cx, cy, visualR * 0.5f, 1.0f, 0.6f, 0.1f, 1.0f, 60);
    } else if (s.stageIndex == 4) {
        if (initialMass < 8.0) {
            SpawnPlanetaryNebulaParticles(cx, cy, 35.0f);
            DrawParticles();
            DrawFilledCircle(cx, cy, 6.0f, 0.4f, 0.8f, 1.0f, 1.0f, 40);
        } else {
            SpawnSupernovaParticles(cx, cy);
            DrawParticles();
            
            static float flashAlpha = 1.0f;
            if (flashAlpha > 0.0f) {
                DrawRect(25, 25, 730, 750, 1.0f, 1.0f, 1.0f, flashAlpha);
                flashAlpha -= 0.04f;
            } else {
                if (rand() % 5 == 0) flashAlpha = 0.15f;
            }
        }
    } else {
        if (initialMass < 8.0) {
            DrawFilledCircle(cx, cy, 35.0f, 0.3f, 0.7f, 1.0f, 0.1f, 40);
            DrawFilledCircle(cx, cy, 15.0f, 0.3f, 0.7f, 1.0f, 0.3f, 40);
            DrawFilledCircle(cx, cy, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f, 40);
        } else if (initialMass <= 20.0) {
            DrawFilledCircle(cx, cy, 12.0f, 0.5f, 0.8f, 1.0f, 0.4f, 30);
            DrawFilledCircle(cx, cy, 4.0f, 1.0f, 1.0f, 1.0f, 1.0f, 30);
            
            static float beamAngle = 0.0f;
            beamAngle += 0.15f;
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(0.3f, 0.7f, 1.0f, 0.5f);
            
            glBegin(GL_TRIANGLES);
            glVertex2f(cx, cy);
            glVertex2f(cx + 250.0f * cosf(beamAngle - 0.12f), cy + 250.0f * sinf(beamAngle - 0.12f));
            glVertex2f(cx + 250.0f * cosf(beamAngle + 0.12f), cy + 250.0f * sinf(beamAngle + 0.12f));
            glEnd();
            
            float oppAngle = beamAngle + PI_F;
            glBegin(GL_TRIANGLES);
            glVertex2f(cx, cy);
            glVertex2f(cx + 250.0f * cosf(oppAngle - 0.12f), cy + 250.0f * sinf(oppAngle - 0.12f));
            glVertex2f(cx + 250.0f * cosf(oppAngle + 0.12f), cy + 250.0f * sinf(oppAngle + 0.12f));
            glEnd();
            glDisable(GL_BLEND);
        } else {
            SpawnBlackHoleDiskParticles(cx, cy, 45.0f);
            DrawParticles();
            
            DrawFilledCircle(cx, cy, 25.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50);
            
            glLineWidth(2.0f);
            DrawCircle(cx, cy, 28.0f, 0.9f, 0.5f, 0.1f, 60);
        }
    }
}

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
    
    DrawRect(0, 0, 1200, 800, 0.05f, 0.05f, 0.08f);
    
    DrawRect(15, 15, 750, 770, 0.09f, 0.10f, 0.15f);
    DrawRectOutline(15, 15, 750, 770, 0.18f, 0.22f, 0.35f, 2.0f);
    
    DrawRect(780, 15, 405, 770, 0.09f, 0.10f, 0.15f);
    DrawRectOutline(780, 15, 405, 770, 0.18f, 0.22f, 0.35f, 2.0f);

    PrintString(800, 750, "CICLO DE VIDA ESTELAR", fontBaseLarge, 1.0f, 1.0f, 1.0f);
    PrintString(800, 730, "Evolucao estelar baseada na massa", fontBaseRegular, 0.6f, 0.6f, 0.7f);

    float massY = 510.0f;
    PrintString(800, massY + 30, "Massa Estelar Inicial:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (DrawButton(800, massY, 80, 30, "0.5 M☉", initialMass == 0.5, mx, my, clicked)) {
        initialMass = 0.5;
        ResetSimulation();
    }
    if (DrawButton(890, massY, 80, 30, "1.0 M☉", initialMass == 1.0, mx, my, clicked)) {
        initialMass = 1.0;
        ResetSimulation();
    }
    if (DrawButton(980, massY, 80, 30, "10 M☉", initialMass == 10.0, mx, my, clicked)) {
        initialMass = 10.0;
        ResetSimulation();
    }
    if (DrawButton(1070, massY, 80, 30, "30 M☉", initialMass == 30.0, mx, my, clicked)) {
        initialMass = 30.0;
        ResetSimulation();
    }

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

    float speedY = 350.0f;
    PrintString(800, speedY + 20, "Velocidade de Evolucao:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (DrawButton(800, speedY, 80, 24, "Lento", simSpeed == 0.002, mx, my, clicked)) simSpeed = 0.002;
    if (DrawButton(890, speedY, 80, 24, "Medio", simSpeed == 0.01, mx, my, clicked)) simSpeed = 0.01;
    if (DrawButton(980, speedY, 80, 24, "Rapido", simSpeed == 0.04, mx, my, clicked)) simSpeed = 0.04;
    
    StarState s = GetStellarProperties(initialMass, agePercent);
    
    if (hrTrail.empty() || hrTrail.back().second != s.lum || hrTrail.back().first != s.temp) {
        hrTrail.push_back({s.temp, s.lum});
    }

    float statsY = 270.0f;
    PrintString(800, statsY + 30, "Dados Físicos Atuais:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    char nameBuf[64], ageBuf[64], tempBuf[64], radBuf[64], lumBuf[64];
    sprintf(nameBuf, "Estágio:  %s", s.stageName.c_str());
    
    if (s.ageYears > 1000.0) {
        sprintf(ageBuf, "Idade: %.2f Bilhoes de anos", s.ageYears / 1000.0);
    } else {
        sprintf(ageBuf, "Idade: %.1f Milhoes de anos", s.ageYears);
    }
    
    if (s.temp == 0.0) sprintf(tempBuf, "Temp: Singularidade (0K)");
    else sprintf(tempBuf, "Temp: %.0f K", s.temp);
    
    if (s.radius < 0.01) sprintf(radBuf, "Raio: %.4f R☉ (Estrela compacta)", s.radius);
    else sprintf(radBuf, "Raio: %.2f R☉", s.radius);
    
    if (s.lum == 0.0) sprintf(lumBuf, "Brilho:  Totalmente Negro (0 L☉)");
    else sprintf(lumBuf, "Brilho: %.2f L☉", s.lum);
    
    PrintString(800, statsY, nameBuf, fontBaseBold, 1.0f, 0.8f, 0.2f);
    PrintString(800, statsY - 20, ageBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 40, tempBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 60, radBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(800, statsY - 80, lumBuf, fontBaseRegular, 0.8f, 0.8f, 0.9f);
    
    DrawRectOutline(30, 410, 720, 310, 0.15f, 0.18f, 0.28f);
    PrintString(45, 695, "Visualizacao da Estrela", fontBaseBold, 0.9f, 0.9f, 1.0f);
    
    DrawStellarVisualizer(390.0f, 560.0f, s);
    DrawCoreOnionShells(180.0f, 200.0f, 100.0f, s.stageIndex, initialMass);
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
        if (dt > 0.1f) dt = 0.1f;
        
        if (isRunning) {
            agePercent += simSpeed * dt;
            if (agePercent > 1.0) {
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
