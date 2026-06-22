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

// True value of PI
const double TRUE_PI = 3.14159265358979323846;

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
        float theta = 2.0f * 3.1415926f * float(i) / float(segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

// Solver states and structures
struct Point {
    float x, y;
    bool inside;
};

struct HistoryItem {
    double iterations;
    double logError;
};

// Base helper to count matching digits of Pi
int CountCorrectDigits(double val) {
    char strVal[32];
    char strTrue[32];
    sprintf(strVal, "%.15f", val);
    sprintf(strTrue, "%.15f", TRUE_PI);
    int count = 0;
    for (int i = 0; i < 17; ++i) {
        if (strVal[i] == strTrue[i]) {
            if (strVal[i] != '.') count++;
        } else {
            break;
        }
    }
    return count;
}

// Gregory-Leibniz
struct LeibnizSolver {
    double value;
    long long k;
    std::vector<HistoryItem> history;
    std::vector<double> recentValues;

    void reset() {
        value = 0.0;
        k = 0;
        history.clear();
        recentValues.clear();
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        double logIter = (k < 1) ? 0.0 : std::log10((double)k);
        history.push_back({logIter, logErr});
        
        recentValues.push_back(value);
        if (recentValues.size() > 200) recentValues.erase(recentValues.begin());
    }

    void step(int count) {
        for (int i = 0; i < count; ++i) {
            double term = 4.0 * (k % 2 == 0 ? 1.0 : -1.0) / (2.0 * k + 1.0);
            value += term;
            k++;
        }
        recordHistory();
    }
};

// Nilakantha
struct NilakanthaSolver {
    double value;
    long long k;
    std::vector<HistoryItem> history;
    std::vector<double> recentValues;

    void reset() {
        value = 3.0;
        k = 1;
        history.clear();
        recentValues.clear();
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        double logIter = (k < 1) ? 0.0 : std::log10((double)k);
        history.push_back({logIter, logErr});
        
        recentValues.push_back(value);
        if (recentValues.size() > 200) recentValues.erase(recentValues.begin());
    }

    void step(int count) {
        for (int i = 0; i < count; ++i) {
            double term = 4.0 / ((2.0 * k) * (2.0 * k + 1.0) * (2.0 * k + 2.0));
            if (k % 2 == 1) value += term;
            else value -= term;
            k++;
        }
        recordHistory();
    }
};

// Monte Carlo
struct MonteCarloSolver {
    double value;
    long long totalPoints;
    long long insidePoints;
    std::vector<Point> points;
    std::vector<HistoryItem> history;

    void reset() {
        value = 0.0;
        totalPoints = 0;
        insidePoints = 0;
        points.clear();
        history.clear();
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        double logIter = (totalPoints < 1) ? 0.0 : std::log10((double)totalPoints);
        history.push_back({logIter, logErr});
    }

    void step(int count) {
        for (int i = 0; i < count; ++i) {
            float x = (float)rand() / RAND_MAX * 2.0f - 1.0f;
            float y = (float)rand() / RAND_MAX * 2.0f - 1.0f;
            bool inside = (x*x + y*y <= 1.0f);
            if (inside) insidePoints++;
            totalPoints++;
            
            if (points.size() < 4000) {
                points.push_back({x, y, inside});
            } else {
                int idx = rand() % points.size();
                points[idx] = {x, y, inside};
            }
        }
        if (totalPoints > 0) value = 4.0 * (double)insidePoints / totalPoints;
        recordHistory();
    }
};

// Archimedes
struct ArchimedesSolver {
    double value;
    double inscribed;
    double circumscribed;
    long long sides;
    std::vector<HistoryItem> history;

    void reset() {
        sides = 6;
        inscribed = 3.0; // Hexagon
        circumscribed = 3.4641016151377544; // 2 * sqrt(3)
        value = (inscribed + circumscribed) / 2.0;
        history.clear();
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        // Let's use log10(sides) as the X axis
        double logSides = std::log10((double)sides);
        history.push_back({logSides, logErr});
    }

    void step() {
        if (sides > 1e16) return;
        double next_circumscribed = (2.0 * circumscribed * inscribed) / (circumscribed + inscribed);
        double next_inscribed = sqrt(next_circumscribed * inscribed);
        
        circumscribed = next_circumscribed;
        inscribed = next_inscribed;
        sides *= 2;
        value = (inscribed + circumscribed) / 2.0;
        recordHistory();
    }
};

// Gauss-Legendre
struct GaussLegendreRow {
    int step;
    double a, b, t, p, piVal;
};

struct GaussLegendreSolver {
    double value;
    double a, b, t, p;
    long long iteration;
    std::vector<HistoryItem> history;
    std::vector<GaussLegendreRow> rows;

    void reset() {
        a = 1.0;
        b = 1.0 / sqrt(2.0);
        t = 0.25;
        p = 1.0;
        iteration = 0;
        value = (a + b) * (a + b) / (4.0 * t);
        history.clear();
        rows.clear();
        rows.push_back({0, a, b, t, p, value});
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        // For GL and Chudnovsky, we'll map iterations to equivalent computational steps
        // Every iteration in GL roughly doubles correct digits (same complexity cost as log)
        // Let's plot X = iteration
        history.push_back({(double)iteration, logErr});
    }

    void step() {
        if (iteration >= 15) return;
        double next_a = (a + b) / 2.0;
        double next_b = sqrt(a * b);
        double next_t = t - p * (a - next_a) * (a - next_a);
        double next_p = 2.0 * p;
        
        a = next_a;
        b = next_b;
        t = next_t;
        p = next_p;
        iteration++;
        value = (a + b) * (a + b) / (4.0 * t);
        
        rows.push_back({(int)iteration, a, b, t, p, value});
        recordHistory();
    }
};

// Chudnovsky
struct ChudnovskyRow {
    int termIndex;
    double Aq;
    double termVal;
    double piVal;
};

struct ChudnovskySolver {
    double value;
    double A;
    double sum;
    long long q;
    std::vector<HistoryItem> history;
    std::vector<ChudnovskyRow> rows;

    void reset() {
        q = 0;
        sum = 0.0;
        A = 1.0 / (640320.0 * sqrt(640320.0));
        double term = A * 13591409.0;
        sum += term;
        value = 1.0 / (12.0 * sum);
        history.clear();
        rows.clear();
        rows.push_back({0, A, term, value});
        recordHistory();
    }

    void recordHistory() {
        double err = std::abs(value - TRUE_PI);
        double logErr = (err < 1e-16) ? -16.0 : std::log10(err);
        history.push_back({(double)q, logErr});
    }

    void step() {
        if (q >= 10) return;
        q++;
        double num = -24.0 * (6.0 * q - 1.0) * (2.0 * q - 1.0) * (6.0 * q - 5.0);
        double den = (double)q * q * q * 262537412640768000.0;
        A *= (num / den);
        
        double term = A * (545140134.0 * q + 13591409.0);
        sum += term;
        value = 1.0 / (12.0 * sum);
        
        rows.push_back({(int)q, A, term, value});
        recordHistory();
    }
};

// Application Global State
LeibnizSolver leibniz;
NilakanthaSolver nilakantha;
MonteCarloSolver monteCarlo;
ArchimedesSolver archimedes;
GaussLegendreSolver gaussLegendre;
ChudnovskySolver chudnovsky;

int activeAlgo = 5; // Start with Chudnovsky (the most efficient)
int activeTab = 0;  // 0 = Visualizer, 1 = Error Graph
bool isRunning = true;
int speedLevel = 3; // 1 to 5

void ResetAll() {
    leibniz.reset();
    nilakantha.reset();
    monteCarlo.reset();
    archimedes.reset();
    gaussLegendre.reset();
    chudnovsky.reset();
}

void UpdateSimulation() {
    if (!isRunning) return;

    // Determine steps per frame based on speed level
    int steps = 1;
    if (speedLevel == 2) steps = 10;
    else if (speedLevel == 3) steps = 100;
    else if (speedLevel == 4) steps = 1000;
    else if (speedLevel == 5) steps = 10000;

    leibniz.step(steps);
    nilakantha.step(steps);
    monteCarlo.step(steps);
    
    // Slow down updates for Archimedes, Gauss-Legendre and Chudnovsky 
    // to make them visible and stable (e.g. step them slowly if we are running)
    static int frameCounter = 0;
    frameCounter++;
    if (frameCounter % 15 == 0) {
        archimedes.step();
        gaussLegendre.step();
        chudnovsky.step();
    }
}

// GUI Drawing Utilities
bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    // Color scheme
    float r = 0.12f, g = 0.14f, b = 0.22f; // normal
    if (active) {
        r = 0.16f; g = 0.40f; b = 0.90f;   // active blue
    } else if (hovered) {
        r = 0.20f; g = 0.23f; b = 0.35f;   // hovered
    }

    DrawRect(x, y, w, h, r, g, b);
    DrawRectOutline(x, y, w, h, 0.3f, 0.4f, 0.6f);
    
    // Center label text
    int labelLen = strlen(label);
    // Simple estimation of text size: ~7px wide per character for normal font
    float textW = labelLen * 7.0f;
    float tx = x + (w - textW) / 2.0f;
    float ty = y + (h - 10.0f) / 2.0f;

    PrintString(tx, ty, label, fontBaseBold, 1.0f, 1.0f, 1.0f);

    return hovered && clicked;
}

// Function to draw digits of computed Pi next to true Pi, color-coded
void DrawDigitsComparison(float x, float y, double computedVal) {
    char strVal[32];
    char strTrue[32];
    sprintf(strVal, "%.15f", computedVal);
    sprintf(strTrue, "%.15f", TRUE_PI);

    // Print Headers
    PrintString(x, y + 40, "Dígitos (Verde = Correto, Vermelho = Incorreto):", fontBaseBold, 0.7f, 0.7f, 0.8f);

    // Print True Pi label
    PrintString(x, y + 20, "Alvo:  3 . 1 4 1 5 9 2 6 5 3 5 8 9 7 9", fontBaseBold, 0.2f, 0.8f, 0.2f);

    // Print Calc Pi label
    PrintString(x, y, "Calc: ", fontBaseBold, 0.7f, 0.7f, 0.8f);
    
    float cx = x + 40.0f;
    // Format computed string into spaced characters and render
    bool correct = true;
    for (int i = 0; i < 17; ++i) {
        char ch = strVal[i];
        char displayStr[2] = { ch, '\0' };
        
        if (i < (int)strlen(strTrue)) {
            if (strVal[i] != strTrue[i]) correct = false;
        } else {
            correct = false;
        }

        float r = 0.9f, g = 0.2f, b = 0.2f; // Red if wrong
        if (correct) r = 0.2f; g = 0.8f; b = 0.2f; // Green if correct
        if (ch == '.') r = 0.7f; g = 0.7f; b = 0.8f; // Gray dot
        
        PrintString(cx, y, displayStr, fontBaseBold, r, g, b);
        cx += 12.0f;
    }
}

// Draw the Convergence Log-Error Graph
void DrawErrorGraph() {
    float x = 40.0f;
    float y = 80.0f;
    float w = 680.0f;
    float h = 600.0f;

    // Background of graph
    DrawRect(x, y, w, h, 0.08f, 0.09f, 0.13f);
    DrawRectOutline(x, y, w, h, 0.2f, 0.25f, 0.35f, 2.0f);

    // Grid Lines for Y-axis (Log Error: from 0 down to -16)
    for (int i = 0; i >= -16; i -= 2) {
        float gy = y + h - ((double)i / -16.0) * h;
        // Horizontal grid line
        glColor4f(0.15f, 0.18f, 0.28f, 0.5f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glVertex2f(x, gy);
        glVertex2f(x + w, gy);
        glEnd();

        // Label
        char lbl[16];
        sprintf(lbl, "10^%d", i);
        PrintString(x - 45.0f, gy - 4.0f, lbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
    }

    // Grid Lines for X-axis (Log Iterations / Steps from 0 to 6)
    // 10^0 = 1, 10^6 = 1,000,000
    for (int i = 0; i <= 6; ++i) {
        float gx = x + ((double)i / 6.0) * w;
        // Vertical grid line
        glColor4f(0.15f, 0.18f, 0.28f, 0.5f);
        glBegin(GL_LINES);
        glVertex2f(gx, y);
        glVertex2f(gx, y + h);
        glEnd();
        
        // Label
        char lbl[16];
        if (i == 0) strcpy(lbl, "1");
        else sprintf(lbl, "10^%d", i);
        PrintString(gx - 15.0f, y - 18.0f, lbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
    }

    PrintString(x + w/2 - 60.0f, y - 35.0f, "Numero de Iteracoes (Log)", fontBaseBold, 0.7f, 0.7f, 0.8f);
    PrintString(x - 30.0f, y + h + 15.0f, "Erro Absoluto (Log10)", fontBaseBold, 0.7f, 0.7f, 0.8f);

    // Draw curves
    auto drawCurve = [&](const std::vector<HistoryItem>& hist, float r, float g, float b, const char* name, int legendIndex) {
        if (hist.empty()) return;
        
        glColor3f(r, g, b);
        glLineWidth(2.5f);
        glBegin(GL_LINE_STRIP);
        for (const auto& pt : hist) {
            // X goes from 0 to 6
            // Y goes from 0 to -16 (so pt.logError / -16.0)
            double px = pt.iterations;
            if (px > 6.0) px = 6.0; // clamp
            if (px < 0.0) px = 0.0;

            double pyVal = pt.logError;
            if (pyVal > 0.0) pyVal = 0.0;
            if (pyVal < -16.0) pyVal = -16.0;

            float screenX = x + (px / 6.0) * w;
            float screenY = y + h - (pyVal / -16.0) * h;
            glVertex2f(screenX, screenY);
        }
        glEnd();

        // Draw legend
        float lx = x + 30.0f + (legendIndex % 3) * 200.0f;
        float ly = y + h - 25.0f - (legendIndex / 3) * 20.0f;
        DrawRect(lx, ly + 2.0f, 15.0f, 8.0f, r, g, b);
        PrintString(lx + 22.0f, ly, name, fontBaseBold, 0.8f, 0.8f, 0.9f);
    };

    drawCurve(leibniz.history, 0.9f, 0.8f, 0.2f, "Leibniz", 0);
    drawCurve(nilakantha.history, 0.8f, 0.3f, 0.9f, "Nilakantha", 1);
    drawCurve(monteCarlo.history, 0.2f, 0.7f, 0.9f, "Monte Carlo", 2);
    drawCurve(archimedes.history, 0.9f, 0.5f, 0.1f, "Arquimedes", 3);
    // Scale X coordinate of Gauss-Legendre and Chudnovsky to align on iterations
    // Since GL and Chudnovsky are mapped directly by step number, we map them as X = steps
    // To show their extreme speed, let's plot their curves
    // Their steps are: 0, 1, 2, 3...
    // We map step index directly to x coordinates: step 1 corresponds to log10(1) = 0, step 10 = log10(10)=1
    // Actually, to make them directly comparable on complexity, let's plot them using step index directly.
    drawCurve(gaussLegendre.history, 0.1f, 0.9f, 0.4f, "Gauss-Legendre", 4);
    drawCurve(chudnovsky.history, 1.0f, 0.2f, 0.2f, "Chudnovsky (Mais Eficiente)", 5);
}

// Main Draw function
void DrawGLScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get mouse state
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(g_hwnd, &mousePos);
    int mx = mousePos.x;
    int my = 800 - mousePos.y; // Invert to match OpenGL coords
    static bool lastMouseState = false;
    bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool clicked = currentMouseState && !lastMouseState;
    lastMouseState = currentMouseState;

    // Set up 2D viewport
    glViewport(0, 0, 1200, 800);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1200, 0, 800, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw Background
    DrawRect(0, 0, 1200, 800, 0.05f, 0.05f, 0.08f);

    // Panels
    DrawRect(15, 15, 750, 770, 0.09f, 0.10f, 0.15f); // Left Panel
    DrawRectOutline(15, 15, 750, 770, 0.18f, 0.22f, 0.35f, 2.0f);

    DrawRect(780, 15, 405, 770, 0.09f, 0.10f, 0.15f); // Right Panel
    DrawRectOutline(780, 15, 405, 770, 0.18f, 0.22f, 0.35f, 2.0f);

    // Title
    PrintString(800, 750, "SIMULAÇÃO DE PI", fontBaseLarge, 1.0f, 1.0f, 1.0f);
    PrintString(800, 730, "Calculo de PI por multiplos modelos", fontBaseRegular, 0.6f, 0.6f, 0.7f);

    // Sidebar: Tabs at Left Panel Header
    if (DrawButton(30, 740, 180, 32, "Visualizador", activeTab == 0, mx, my, clicked)) activeTab = 0;
    if (DrawButton(220, 740, 180, 32, "Grafico de Erro (Log)", activeTab == 1, mx, my, clicked)) {
        activeTab = 1;
    }

    // Sidebar: Algorithm Buttons
    float btnY = 510.0f;
    const char* algoNames[] = {
        "1. Gregory-Leibniz",
        "2. Nilakantha",
        "3. Monte Carlo",
        "4. Poligono de Arquimedes",
        "5. Gauss-Legendre",
        "6. Chudnovsky (Mais Eficiente)"
    };
    
    PrintString(800, btnY + 30, "Selecione o Modelo Ativo:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    for (int i = 0; i < 6; ++i) {
        if (DrawButton(800, btnY - i * 36, 365, 30, algoNames[i], activeAlgo == i, mx, my, clicked)) {
            activeAlgo = i;
        }
    }
    
    // Sidebar: Controls
    float ctrlY = 240.0f;
    PrintString(800, ctrlY + 30, "Controles de Simulacao:", fontBaseBold, 0.8f, 0.8f, 0.9f);
    
    if (DrawButton(800, ctrlY, 110, 30, isRunning ? "Pausar" : "Iniciar", false, mx, my, clicked)) {
        isRunning = !isRunning;
    }
    
    // Step Button (Manual Step)
    if (DrawButton(920, ctrlY, 110, 30, "Passo a Passo", false, mx, my, clicked)) {
        isRunning = false;
        leibniz.step(1);
        nilakantha.step(1);
        monteCarlo.step(1);
        archimedes.step();
        gaussLegendre.step();
        chudnovsky.step();
    }
    
    if (DrawButton(1040, ctrlY, 125, 30, "Reiniciar Tudo", false, mx, my, clicked)) ResetAll();

    // Speed Selector (Only affects Leibniz, Nilakantha, Monte Carlo)
    float speedY = 180.0f;
    PrintString(800, speedY + 20, "Velocidade de Iteracao (Leibniz/MC):", fontBaseBold, 0.8f, 0.8f, 0.9f);
    char speedText[32];
    sprintf(speedText, "Nivel: %d (%d iter/frame)", speedLevel, 
            (speedLevel==1?1:(speedLevel==2?10:(speedLevel==3?100:(speedLevel==4?1000:10000)))));
    PrintString(800, speedY, speedText, fontBaseRegular, 0.7f, 0.7f, 0.8f);
    
    for (int i = 1; i <= 5; ++i) {
        char valStr[4];
        sprintf(valStr, "%d", i);
        if (DrawButton(1000 + (i-1)*32, speedY, 28, 24, valStr, speedLevel == i, mx, my, clicked)) {
            speedLevel = i;
        }
    }

    // Get Active Solver details
    double currentVal = 0.0;
    long long currentSteps = 0;
    int correctDigits = 0;
    
    if (activeAlgo == 0) {
        currentVal = leibniz.value;
        currentSteps = leibniz.k;
    } else if (activeAlgo == 1) {
        currentVal = nilakantha.value;
        currentSteps = nilakantha.k;
    } else if (activeAlgo == 2) {
        currentVal = monteCarlo.value;
        currentSteps = monteCarlo.totalPoints;
    } else if (activeAlgo == 3) {
        currentVal = archimedes.value;
        currentSteps = archimedes.sides;
    } else if (activeAlgo == 4) {
        currentVal = gaussLegendre.value;
        currentSteps = gaussLegendre.iteration;
    } else if (activeAlgo == 5) {
        currentVal = chudnovsky.value; 
        currentSteps = chudnovsky.q;
    }
    
    correctDigits = CountCorrectDigits(currentVal);
    
    // Display calculations in Right Panel
    float readY = 90.0f;
    DrawDigitsComparison(800, readY, currentVal);

    char valBuf[64], stepBuf[64], errBuf[64];
    sprintf(valBuf, "Valor Estimado:  %.15f", currentVal);
    
    if (activeAlgo == 3) sprintf(stepBuf, "Lados do Poligono: %lld", currentSteps);
    else if (activeAlgo == 4 || activeAlgo == 5) sprintf(stepBuf, "Iteracoes / Termos: %lld", currentSteps);
    else sprintf(stepBuf, "Iteracoes: %lld", currentSteps);
    
    double absErr = std::abs(currentVal - TRUE_PI);
    sprintf(errBuf, "Erro Absoluto: %e", absErr);

    PrintString(800, readY - 30, valBuf, fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(800, readY - 50, stepBuf, fontBaseRegular, 0.7f, 0.7f, 0.8f);
    PrintString(800, readY - 70, errBuf, fontBaseRegular, 0.9f, 0.5f, 0.5f);

    // Left Panel Rendering depending on activeTab
    if (activeTab == 1) {
        DrawErrorGraph();
    } else {
        // Visualizer mode
        if (activeAlgo == 2) {
            // Monte Carlo Visualizer
            // Square bounds: x=140 to 640 (width 500, height 500), y=140 to 640
            float sqX = 140.0f;
            float sqY = 140.0f;
            float sqW = 500.0f;
            float sqH = 500.0f;
            float cx = sqX + sqW / 2.0f;
            float cy = sqY + sqH / 2.0f;
            float r = sqW / 2.0f;
            
            DrawRect(sqX, sqY, sqW, sqH, 0.08f, 0.09f, 0.13f);
            DrawRectOutline(sqX, sqY, sqW, sqH, 0.3f, 0.35f, 0.45f, 2.0f);
            
            // Draw Circle Outline
            DrawCircle(cx, cy, r, 0.16f, 0.40f, 0.90f, 100);
            
            // Draw points
            glPointSize(1.5f);
            glBegin(GL_POINTS);
            for (const auto& pt : monteCarlo.points) {
                // pt.x, pt.y are in range [-1, 1]
                float px = cx + pt.x * r;
                float py = cy + pt.y * r;
                if (pt.inside) {
                    glColor3f(0.2f, 0.8f, 0.3f); // Inside green
                } else {
                    glColor3f(0.8f, 0.2f, 0.2f); // Outside red
                }
                glVertex2f(px, py);
            }
            glEnd();
            
            // Render MC Info inside
            char mcInfo1[64], mcInfo2[64];
            sprintf(mcInfo1, "Pontos no Circulo (Verde): %lld", monteCarlo.insidePoints);
            sprintf(mcInfo2, "Total de Pontos: %lld", monteCarlo.totalPoints);
            
            PrintString(sqX + 15.0f, sqY + sqH - 25.0f, "Simulacao de Monte Carlo", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(sqX + 15.0f, sqY + sqH - 45.0f, mcInfo1, fontBaseRegular, 0.2f, 0.8f, 0.3f);
            PrintString(sqX + 15.0f, sqY + sqH - 65.0f, mcInfo2, fontBaseRegular, 0.8f, 0.8f, 0.9f);
            PrintString(sqX + 15.0f, sqY + 15.0f, "Formula: PI ~ 4 * (Pontos no Circulo / Total)", fontBaseBold, 0.8f, 0.8f, 0.9f);
            
        } else if (activeAlgo == 3) {
            // Archimedes Visualizer
            float cx = 390.0f;
            float cy = 390.0f;
            float r = 220.0f;
            
            // Draw Circle
            DrawCircle(cx, cy, r, 0.4f, 0.4f, 0.5f, 120);
            
            // Draw Inscribed Polygon
            long long s = archimedes.sides;
            // Limit drawing density for high side counts to avoid visual mess
            int drawSides = (s > 1024) ? 1024 : (int)s;
            
            // Inscribed
            glColor3f(0.2f, 0.7f, 0.9f); // Cyan
            glLineWidth(1.5f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < drawSides; ++i) {
                float theta = 2.0f * 3.14159265f * float(i) / float(drawSides);
                float px = cx + r * cosf(theta);
                float py = cy + r * sinf(theta);
                glVertex2f(px, py);
            }
            glEnd();
            
            // Circumscribed
            glColor3f(0.9f, 0.4f, 0.2f); // Orange
            glLineWidth(1.5f);
            glBegin(GL_LINE_LOOP);
            // Outer radius = r / cos(pi / sides)
            float rOuter = r / cosf(3.14159265f / float(s));
            for (int i = 0; i < drawSides; ++i) {
                float theta = 2.0f * 3.14159265f * float(i) / float(drawSides);
                float px = cx + rOuter * cosf(theta);
                float py = cy + rOuter * sinf(theta);
                glVertex2f(px, py);
            }
            glEnd();
            
            // Text boxes
            PrintString(50.0f, 690.0f, "Metodo de Arquimedes (Poligonos Inscritos e Circunscritos)", fontBaseBold, 1.0f, 1.0f, 1.0f);
            
            char archText1[64], archText2[64];
            sprintf(archText1, "Limite Inferior (Inscrito): %.14f", archimedes.inscribed);
            sprintf(archText2, "Limite Superior (Circunscrito): %.14f", archimedes.circumscribed);
            
            PrintString(50.0f, 110.0f, archText1, fontBaseBold, 0.2f, 0.7f, 0.9f);
            PrintString(50.0f, 90.0f, archText2, fontBaseBold, 0.9f, 0.4f, 0.2f);
            
            char sideStr[64];
            sprintf(sideStr, "Poligono de %lld lados (passos de duplicacao)", archimedes.sides);
            PrintString(50.0f, 670.0f, sideStr, fontBaseRegular, 0.7f, 0.7f, 0.8f);
            
        } else if (activeAlgo == 0 || activeAlgo == 1) {
            // Leibniz & Nilakantha Waveform Visualizer
            float gx = 50.0f;
            float gy = 160.0f;
            float gw = 680.0f;
            float gh = 450.0f;
            
            DrawRect(gx, gy, gw, gh, 0.08f, 0.09f, 0.13f);
            DrawRectOutline(gx, gy, gw, gh, 0.2f, 0.25f, 0.35f, 2.0f);
            
            // Draw Target green line
            float pyTarget = gy + gh / 2.0f; // Target Pi line
            glColor3f(0.2f, 0.8f, 0.3f);
            glLineWidth(1.5f);
            glBegin(GL_LINES);
            glVertex2f(gx, pyTarget);
            glVertex2f(gx + gw, pyTarget);
            glEnd();
            PrintString(gx + 10.0f, pyTarget + 5.0f, "PI Real (3.14159...)", fontBaseBold, 0.2f, 0.8f, 0.3f);
            
            // Draw waveform of estimates
            const std::vector<double>& vals = (activeAlgo == 0) ? leibniz.recentValues : nilakantha.recentValues;
            
            if (vals.size() > 1) {
                // Find scale range
                double minVal = 99.0, maxVal = -99.0;
                for (double v : vals) {
                    if (v < minVal) minVal = v;
                    if (v > maxVal) maxVal = v;
                }
                
                // Add margins to scale
                double range = maxVal - minVal;
                if (range < 0.0001) range = 0.0001; // prevent division by zero
                double scaleMin = minVal - range * 0.1;
                double scaleMax = maxVal + range * 0.1;
                
                // Draw wave
                if (activeAlgo == 0) glColor3f(0.9f, 0.8f, 0.2f); // Leibniz yellow
                else glColor3f(0.8f, 0.3f, 0.9f); // Nilakantha purple
                
                glLineWidth(2.0f);
                glBegin(GL_LINE_STRIP);
                for (size_t i = 0; i < vals.size(); ++i) {
                    float vx = gx + ((float)i / (vals.size() - 1)) * gw;
                    float vy = gy + ((vals[i] - scaleMin) / (scaleMax - scaleMin)) * gh;
                    glVertex2f(vx, vy);
                }
                glEnd();
                
                // Label scale
                char minLbl[32], maxLbl[32];
                sprintf(minLbl, "Min: %.6f", scaleMin);
                sprintf(maxLbl, "Max: %.6f", scaleMax);
                PrintString(gx - 45.0f, gy, minLbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
                PrintString(gx - 45.0f, gy + gh - 15.0f, maxLbl, fontBaseRegular, 0.6f, 0.6f, 0.7f);
            }
            
            if (activeAlgo == 0) {
                PrintString(50.0f, 690.0f, "Serie de Gregory-Leibniz", fontBaseBold, 1.0f, 1.0f, 1.0f);
                PrintString(50.0f, 670.0f, "Oscilacao lenta e convergencia linear (1/N)", fontBaseRegular, 0.7f, 0.7f, 0.8f);
                PrintString(50.0f, 110.0f, "Formula: PI = 4 * (1 - 1/3 + 1/5 - 1/7 + 1/9 - ...)", fontBaseBold, 0.8f, 0.8f, 0.9f);
            } else {
                PrintString(50.0f, 690.0f, "Serie de Nilakantha", fontBaseBold, 1.0f, 1.0f, 1.0f);
                PrintString(50.0f, 670.0f, "Oscilacao e convergencia consideravelmente mais rapida (1/N^3)", fontBaseRegular, 0.7f, 0.7f, 0.8f);
                PrintString(50.0f, 110.0f, "Formula: PI = 3 + 4/(2*3*4) - 4/(4*5*6) + 4/(6*7*8) - ...", fontBaseBold, 0.8f, 0.8f, 0.9f);
            }
            
        } else if (activeAlgo == 4) {
            // Gauss-Legendre step table
            PrintString(50.0f, 690.0f, "Algoritmo Gauss-Legendre (Convergencia Quadratica)", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(50.0f, 670.0f, "O numero de digitos corretos dobra a cada passo!", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            
            // Draw Table
            float tx = 40.0f;
            float ty = 600.0f;
            float tw = 700.0f;
            
            // Header
            DrawRect(tx, ty, tw, 28.0f, 0.15f, 0.20f, 0.35f);
            PrintString(tx + 10.0f, ty + 8.0f, "Passo", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 60.0f, ty + 8.0f, "a_n", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 180.0f, ty + 8.0f, "b_n", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 300.0f, ty + 8.0f, "t_n", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 420.0f, ty + 8.0f, "p_n", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 510.0f, ty + 8.0f, "Estimativa de PI", fontBaseBold, 1.0f, 1.0f, 1.0f);
            
            // Rows
            for (size_t i = 0; i < gaussLegendre.rows.size() && i < 15; ++i) {
                float ry = ty - 30.0f - i * 30.0f;
                // Alternate row color
                if (i % 2 == 0) {
                    DrawRect(tx, ry - 4.0f, tw, 28.0f, 0.12f, 0.13f, 0.18f);
                } else {
                    DrawRect(tx, ry - 4.0f, tw, 28.0f, 0.10f, 0.11f, 0.16f);
                }
                
                char stepStr[16], aStr[32], bStr[32], tStr[32], pStr[32], piStr[32];
                const auto& r = gaussLegendre.rows[i];
                sprintf(stepStr, "%d", r.step);
                sprintf(aStr, "%.8f", r.a);
                sprintf(bStr, "%.8f", r.b);
                sprintf(tStr, "%.8f", r.t);
                sprintf(pStr, "%.0f", r.p);
                sprintf(piStr, "%.15f", r.piVal);
                
                PrintString(tx + 10.0f, ry + 4.0f, stepStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 60.0f, ry + 4.0f, aStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 180.0f, ry + 4.0f, bStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 300.0f, ry + 4.0f, tStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 420.0f, ry + 4.0f, pStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                
                // Color code the PI digits in table
                int match = CountCorrectDigits(r.piVal);
                float pr = 0.9f, pg = 0.2f, pb = 0.2f;
                if (match >= 15) { pr = 0.2f; pg = 0.8f; pb = 0.2f; } // Full green
                else if (match > 0) { pr = 0.9f; pg = 0.7f; pb = 0.1f; } // Orange-ish
                PrintString(tx + 510.0f, ry + 4.0f, piStr, fontBaseBold, pr, pg, pb);
            }
            
            // Formula display at bottom
            float fy = 110.0f;
            PrintString(50.0f, fy, "Atualizacao Gauss-Legendre:", fontBaseBold, 0.8f, 0.8f, 0.9f);
            PrintString(50.0f, fy - 20.0f, "  a_{n+1} = (a_n + b_n) / 2", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(50.0f, fy - 40.0f, "  b_{n+1} = sqrt(a_n * b_n)", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(300.0f, fy - 20.0f, "  t_{n+1} = t_n - p_n * (a_n - a_{n+1})^2", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(300.0f, fy - 40.0f, "  p_{n+1} = 2 * p_n", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(50.0f, fy - 70.0f, "Estimativa: PI_n = (a_n + b_n)^2 / (4 * t_n)", fontBaseBold, 0.8f, 0.8f, 0.9f);
            
        } else if (activeAlgo == 5) {
            // Chudnovsky step table
            PrintString(50.0f, 690.0f, "Algoritmo de Chudnovsky (Mais Eficiente Disponivel)", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(50.0f, 670.0f, "Cada termo adiciona 14 digitos corretos de PI!", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            
            // Draw Table
            float tx = 40.0f;
            float ty = 600.0f;
            float tw = 700.0f;
            
            // Header
            DrawRect(tx, ty, tw, 28.0f, 0.35f, 0.15f, 0.15f);
            PrintString(tx + 10.0f, ty + 8.0f, "Termo (q)", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 90.0f, ty + 8.0f, "Multiplicador A_q", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 290.0f, ty + 8.0f, "Valor do Termo T_q", fontBaseBold, 1.0f, 1.0f, 1.0f);
            PrintString(tx + 510.0f, ty + 8.0f, "Estimativa de PI", fontBaseBold, 1.0f, 1.0f, 1.0f);
            
            // Rows
            for (size_t i = 0; i < chudnovsky.rows.size() && i < 15; ++i) {
                float ry = ty - 30.0f - i * 30.0f;
                // Alternate row color
                if (i % 2 == 0) {
                    DrawRect(tx, ry - 4.0f, tw, 28.0f, 0.12f, 0.13f, 0.18f);
                } else {
                    DrawRect(tx, ry - 4.0f, tw, 28.0f, 0.10f, 0.11f, 0.16f);
                }
                
                char qStr[16], aStr[64], termStr[64], piStr[32];
                const auto& r = chudnovsky.rows[i];
                sprintf(qStr, "%d", r.termIndex);
                sprintf(aStr, "%e", r.Aq);
                sprintf(termStr, "%e", r.termVal);
                sprintf(piStr, "%.15f", r.piVal);
                
                PrintString(tx + 10.0f, ry + 4.0f, qStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 90.0f, ry + 4.0f, aStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                PrintString(tx + 290.0f, ry + 4.0f, termStr, fontBaseRegular, 0.8f, 0.8f, 0.9f);
                
                int match = CountCorrectDigits(r.piVal);
                float pr = 0.9f, pg = 0.2f, pb = 0.2f;
                if (match >= 15) { pr = 0.2f; pg = 0.8f; pb = 0.2f; } // Full green
                else if (match > 0) { pr = 0.9f; pg = 0.7f; pb = 0.1f; }
                PrintString(tx + 510.0f, ry + 4.0f, piStr, fontBaseBold, pr, pg, pb);
            }
            
            // Formula display at bottom
            float fy = 110.0f;
            PrintString(50.0f, fy, "Formula de Chudnovsky:", fontBaseBold, 0.8f, 0.8f, 0.9f);
            PrintString(50.0f, fy - 20.0f, "  1/PI = 12 * Sum_{q=0}^{inf} T_q", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(50.0f, fy - 40.0f, "  T_q = [(-1)^q * (6q)! * (545140134q + 13591409)] / [(3q)! * (q!)^3 * 640320^(3q+1.5)]", fontBaseRegular, 0.7f, 0.7f, 0.8f);
            PrintString(50.0f, fy - 70.0f, "Nota: Este e o algoritmo padrão usado pelos recordistas mundiais.", fontBaseBold, 0.8f, 0.8f, 0.9f);
        }
    }

    SwapBuffers(g_hdc);
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    
    if (!CreateGLWindow("Visualizacao do Calculo de PI", 1200, 800)) {
        printf("Falha ao criar janela OpenGL.\n");
        return 1;
    }

    BuildFonts();
    ResetAll();

    // Loop
    while (true) {
        ProcessMessages();
        
        UpdateSimulation();
        DrawGLScene();
        
        // Cap frame rate at 60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
