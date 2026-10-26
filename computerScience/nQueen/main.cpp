#include "window.h"
#include <gl/gl.h>
#include <windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <excpt.h>

// TRATADOR DE EXCECOES WINDOWS
static LONG WINAPI MyUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo) {
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    const char* desc = "Erro desconhecido";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            desc = "Acesso a memoria invalido (Segmentation Fault)";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            desc = "Instrucao invalida";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            desc = "Divisao por zero";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            desc = "Estouro de pilha";
            break;
        default:
            desc = "Excecao nao tratada";
            break;
    }

    char msg[512];
    sprintf(msg, "ERRO CRITICO\n\nCodigo: 0x%08lX\nDescricao: %s\n\nO programa sera encerrado.", code, desc);
    MessageBoxA(NULL, msg, "N Queen - Erro", MB_OK | MB_ICONERROR);

    FILE* log = std::fopen("nQueen_error.log", "w");
    if (log) {
        fprintf(log, "Codigo: 0x%08lX\nDescricao: %s\n", code, desc);
        std::fclose(log);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

const int WINDOW_W = 1100;
const int WINDOW_H = 700;

const int BOARD_OFFSET_X = 40;
const int BOARD_OFFSET_Y = 50;
const int BOARD_MAX_SIZE = 580;

const int MIN_N = 4;
const int MAX_N = 64;

int g_n = 8;
std::atomic<int> g_nAtomic{8};

int g_board[MAX_N][MAX_N] = {{0}};
std::mutex g_boardMutex;

std::atomic<int> g_curRow{-1};
std::atomic<int> g_curCol{-1};

enum SolveStatus { IDLE, SOLVING, SOLVED, PAUSED };
std::atomic<SolveStatus> g_status{IDLE};

std::atomic<long long> g_steps{0};
std::atomic<int> g_solutionsFound{0};
std::atomic<bool> g_stopRequested{false};

std::atomic<int> g_delayMs{100};

std::atomic<bool> g_stepMode{false};
std::atomic<bool> g_stepTrigger{false};

std::thread g_solveThread;

const int g_nOptions[] = {4, 6, 8, 10, 12, 16, 24, 32, 64};
const int g_nOptionsCount = sizeof(g_nOptions) / sizeof(g_nOptions[0]);

static GLuint fontBaseRegular = 0;
static GLuint fontBaseBold = 0;
static GLuint fontBaseLarge = 0;

static void BuildFonts() {
    if (!g_hdc) return;

    if (fontBaseRegular) glDeleteLists(fontBaseRegular, 96);
    if (fontBaseBold) glDeleteLists(fontBaseBold, 96);
    if (fontBaseLarge) glDeleteLists(fontBaseLarge, 96);

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
    HFONT fontLarge = CreateFont(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI");
    SelectObject(g_hdc, fontLarge);
    wglUseFontBitmaps(g_hdc, 32, 96, fontBaseLarge);

    SelectObject(g_hdc, oldFont);
    DeleteObject(fontReg);
    DeleteObject(fontBold);
    DeleteObject(fontLarge);
}

static void PrintString(float x, float y, const char* str, GLuint base, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    if (!base || !str) return;
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)std::strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

static void DrawRect(float x, float y, float w, float h, float r, float gcol, float b, float a = 1.0f) {
    glColor4f(r, gcol, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawRectOutline(float x, float y, float w, float h, float r, float gcol, float b, float thickness = 1.5f) {
    glColor3f(r, gcol, b);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static bool DrawButton(float x, float y, float w, float h, const char* label, bool active, int mx, int my, bool clicked) {
    bool hovered = (mx >= x && mx <= x + w && my >= y && my <= y + h);

    float r = 0.12f, gcol = 0.14f, b = 0.22f;
    if (active) {
        r = 0.20f; gcol = 0.28f; b = 0.45f;
    } else if (hovered) {
        r = 0.18f; gcol = 0.22f; b = 0.38f;
    }

    DrawRect(x, y, w, h, r, gcol, b);
    DrawRectOutline(x, y, w, h, 0.4f, 0.5f, 0.7f, 1.0f);

    float lx = x + (w - 7.5f * (float)std::strlen(label)) * 0.5f;
    float ly = y + h * 0.35f;
    PrintString(lx, ly, label, fontBaseBold, 1.0f, 1.0f, 1.0f);

    return hovered && clicked;
}

static void DrawQueen(float cx, float cy, float size, float r, float gcol, float b) {
    if (size < 12.0f) {
        glColor3f(r, gcol, b);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 8; ++i) {
            float angle = 3.1415926f * 2.0f * i / 8.0f;
            glVertex2f(cx + std::cos(angle) * size * 0.5f, cy + std::sin(angle) * size * 0.5f);
        }
        glEnd();

        glColor3f(0.1f, 0.1f, 0.1f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= 8; ++i) {
            float angle = 3.1415926f * 2.0f * i / 8.0f;
            glVertex2f(cx + std::cos(angle) * size * 0.5f, cy + std::sin(angle) * size * 0.5f);
        }
        glEnd();
        return;
    }

    glColor3f(r, gcol, b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 12; ++i) {
        float angle = 3.1415926f * 2.0f * i / 12.0f;
        glVertex2f(cx + std::cos(angle) * size * 0.55f, cy + std::sin(angle) * size * 0.55f);
    }
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(cx - size * 0.35f, cy + size * 0.35f);
    glVertex2f(cx + size * 0.35f, cy + size * 0.35f);
    glVertex2f(cx, cy + size * 0.85f);
    glEnd();

    glColor3f(0.1f, 0.1f, 0.1f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= 12; ++i) {
        float angle = 3.1415926f * 2.0f * i / 12.0f;
        glVertex2f(cx + std::cos(angle) * size * 0.55f, cy + std::sin(angle) * size * 0.55f);
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 0.8f);
    glBegin(GL_LINES);
    glVertex2f(cx - size * 0.2f, cy + size * 0.15f);
    glVertex2f(cx + size * 0.2f, cy + size * 0.15f);
    glEnd();
}

bool IsSafe(int board[MAX_N][MAX_N], int n, int row, int col) {
    for (int i = 0; i < row; ++i) {
        if (board[i][col] == 1) return false;
    }
    for (int i = row, j = col; i >= 0 && j >= 0; --i, --j) {
        if (board[i][j] == 1) return false;
    }
    for (int i = row, j = col; i >= 0 && j < n; --i, ++j) {
        if (board[i][j] == 1) return false;
    }
    return true;
}

bool SolveNQueen(int board[MAX_N][MAX_N], int n, int row) {
    if (g_stopRequested.load()) return false;

    if (row == n) {
        g_solutionsFound.fetch_add(1);
        g_status.store(SOLVED);
        return true;
    }

    for (int col = 0; col < n; ++col) {
        if (g_stopRequested.load()) return false;

        g_curRow.store(row);
        g_curCol.store(col);
        g_steps.fetch_add(1);

        if (g_stepMode.load()) {
            while (g_stepMode.load() && !g_stepTrigger.load() && !g_stopRequested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            g_stepTrigger.store(false);
        }

        while (g_status.load() == PAUSED && !g_stopRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (g_stopRequested.load()) return false;

        if (IsSafe(board, n, row, col)) {
            board[row][col] = 1;
            {
                std::lock_guard<std::mutex> lock(g_boardMutex);
                g_board[row][col] = 1;
            }

            int delay = g_delayMs.load();
            if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (SolveNQueen(board, n, row + 1)) return true;

            board[row][col] = 0;
            {
                std::lock_guard<std::mutex> lock(g_boardMutex);
                g_board[row][col] = 0;
            }

            g_curRow.store(row);
            g_curCol.store(col);
            g_steps.fetch_add(1);

            if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }

    return false;
}

void ResetBoard(int board[MAX_N][MAX_N], int n) {
    std::lock_guard<std::mutex> lock(g_boardMutex);
    for (int i = 0; i < MAX_N; ++i) {
        for (int j = 0; j < MAX_N; ++j) board[i][j] = 0;
    }
}

void RunSolver() {
    int n = g_nAtomic.load();
    if (n < MIN_N) n = MIN_N;
    if (n > MAX_N) n = MAX_N;

    int localBoard[MAX_N][MAX_N] = {{0}};
    {
        std::lock_guard<std::mutex> lock(g_boardMutex);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) localBoard[i][j] = g_board[i][j];
    }

    g_solutionsFound.store(0);
    g_steps.store(0);
    g_status.store(SOLVING);
    g_stopRequested.store(false);

    SolveNQueen(localBoard, n, 0);

    {
        std::lock_guard<std::mutex> lock(g_boardMutex);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) g_board[i][j] = localBoard[i][j];
    }

    if (g_status.load() != SOLVED && !g_stopRequested.load()) {
        g_status.store(IDLE);
    }
}

void StartSolver() {
    if (g_status.load() == SOLVING) return;
    g_stopRequested.store(false);
    if (g_solveThread.joinable()) g_solveThread.join();

    ResetBoard(g_board, g_nAtomic.load());
    g_solutionsFound.store(0);
    g_steps.store(0);
    g_solveThread = std::thread(RunSolver);
}

void StopSolver() {
    g_stopRequested.store(true);
    if (g_solveThread.joinable()) g_solveThread.join();
    g_status.store(IDLE);
}

static void DrawBoard() {
    int n = g_nAtomic.load();
    if (n < MIN_N) n = MIN_N;
    if (n > MAX_N) n = MAX_N;

    float cellSize = (float)BOARD_MAX_SIZE / (float)n;
    float boardW = cellSize * n;
    float boardH = cellSize * n;
    float startX = (float)BOARD_OFFSET_X;
    float startY = (float)BOARD_OFFSET_Y;

    int curRow = g_curRow.load();
    int curCol = g_curCol.load();

    int localBoard[MAX_N][MAX_N] = {{0}};
    {
        std::lock_guard<std::mutex> lock(g_boardMutex);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) localBoard[i][j] = g_board[i][j];
    }

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            float x = startX + j * cellSize;
            float y = startY + i * cellSize;

            bool isDark = ((i + j) % 2 == 1);
            float r, gcol, b;

            bool isCursor = (i == curRow && j == curCol);

            if (isCursor) {
                r = 1.0f;
                gcol = 0.85f;
                b = 0.2f;
            } else if (isDark) {
                r = 0.30f;
                gcol = 0.40f;
                b = 0.55f;
            } else {
                r = 0.45f;
                gcol = 0.55f;
                b = 0.70f;
            }

            if (localBoard[i][j] == 1) {
                r = 0.25f;
                gcol = 0.75f;
                b = 0.35f;
            }

            DrawRect(x, y, cellSize, cellSize, r, gcol, b);
            DrawRectOutline(x, y, cellSize, cellSize, 0.6f, 0.7f, 0.85f, n > 32 ? 0.5f : 1.0f);

            if (localBoard[i][j] == 1) {
                float cx = x + cellSize * 0.5f;
                float cy = y + cellSize * 0.5f;
                float qSize = cellSize * 0.75f;
                DrawQueen(cx, cy, qSize, 0.95f, 0.9f, 0.2f);
            }
        }
    }

    DrawRectOutline(startX - 2.0f, startY - 2.0f, boardW + 4.0f, boardH + 4.0f, 0.9f, 0.9f, 1.0f, 2.0f);

    int step = 1;
    if (n > 32) step = 8;
    else if (n > 16) step = 4;
    else if (n > 12) step = 2;

    // Rotulos de coluna (em cima do tabuleiro)
    glColor3f(0.85f, 0.85f, 0.85f);
    for (int j = 0; j < n; j += step) {
        char label[8];
        sprintf(label, "%d", j);
        float lx = startX + j * cellSize + cellSize * 0.5f - 4.0f;
        float ly = startY + boardH + 4.0f;
        PrintString(lx, ly, label, fontBaseRegular, 0.85f, 0.85f, 0.85f);
    }
    // Rotulos de linha (esquerda do tabuleiro)
    for (int i = 0; i < n; i += step) {
        char label[8];
        sprintf(label, "%d", i);
        float lx = startX - 22.0f;
        float ly = startY + i * cellSize + cellSize * 0.35f;
        PrintString(lx, ly, label, fontBaseRegular, 0.85f, 0.85f, 0.85f);
    }
}

static void DrawHelpCard(float cx, float cy) {
    float w = 380.0f, h = 280.0f;
    float x = cx - w * 0.5f;
    float y = cy - h * 0.5f;
    DrawRect(x, y, w, h, 0.10f, 0.12f, 0.18f);
    DrawRectOutline(x, y, w, h, 0.5f, 0.6f, 0.8f, 1.5f);

    PrintString(x + 15, y + h - 30, "AJUDA - N RAINHAS", fontBaseBold, 1.0f, 1.0f, 0.3f);
    PrintString(x + 15, y + h - 55, "Objetivo: posicionar N rainhas em", fontBaseRegular, 0.9f, 0.9f, 0.9f);
    PrintString(x + 15, y + h - 70, "um tabuleiro NxN sem que nenhuma", fontBaseRegular, 0.9f, 0.9f, 0.9f);
    PrintString(x + 15, y + h - 85, "ataque outra (linha, coluna ou diagonal).", fontBaseRegular, 0.9f, 0.9f, 0.9f);
    PrintString(x + 15, y + h - 110, "CONTROLES:", fontBaseBold, 1.0f, 1.0f, 0.3f);
    PrintString(x + 15, y + h - 130, "ESPACO  - Iniciar / Pausar solucao", fontBaseRegular, 0.85f, 0.85f, 0.85f);
    PrintString(x + 15, y + h - 145, "R       - Reiniciar tabuleiro", fontBaseRegular, 0.85f, 0.85f, 0.85f);
    PrintString(x + 15, y + h - 160, "P       - Passo a passo (ativar/desativar)", fontBaseRegular, 0.85f, 0.85f, 0.85f);
    PrintString(x + 15, y + h - 175, "SETAS   - Ajustar velocidade (ms)", fontBaseRegular, 0.85f, 0.85f, 0.85f);
    PrintString(x + 15, y + h - 190, "1..9    - Alterar N (4,6,8,10,12,16,24,32,64)", fontBaseRegular, 0.85f, 0.85f, 0.85f);
    PrintString(x + 15, y + h - 205, "H       - Mostrar/ocultar esta ajuda", fontBaseRegular, 0.85f, 0.85f, 0.85f);
}

static void DrawUI(int mx, int my, bool clicked, bool showHelp) {
    float panelX = (float)BOARD_OFFSET_X + (float)BOARD_MAX_SIZE + 30.0f;
    float panelW = (float)(WINDOW_W - (int)panelX - 25);
    float panelY = 40.0f;
    float panelH = (float)(WINDOW_H - 80);
    DrawRect(panelX, panelY, panelW, panelH, 0.08f, 0.10f, 0.15f);
    DrawRectOutline(panelX, panelY, panelW, panelH, 0.4f, 0.5f, 0.7f, 1.0f);

    float ty = panelY + panelH - 35.0f;
    PrintString(panelX + 15, ty, "N RAINHAS - CONTROLES", fontBaseBold, 1.0f, 0.8f, 0.3f);
    ty -= 25.0f;

    PrintString(panelX + 15, ty, "Status:", fontBaseRegular, 0.8f, 0.8f, 0.8f);
    ty -= 20.0f;
    const char* statusStr;
    float sr = 0.8f, sg = 0.8f, sb = 0.8f;
    switch (g_status.load()) {
        case IDLE:
            statusStr = "Ocioso";
            sr = 0.8f; sg = 0.8f; sb = 0.8f;
            break;
        case SOLVING:
            statusStr = "Resolvendo...";
            sr = 0.9f; sg = 0.9f; sb = 0.3f;
            break;
        case SOLVED:
            statusStr = "SOLUCIONADO!";
            sr = 0.3f; sg = 0.9f; sb = 0.3f;
            break;
        case PAUSED:
            statusStr = "Pausado";
            sr = 0.9f; sg = 0.5f; sb = 0.3f;
            break;
    }
    PrintString(panelX + 15, ty, statusStr, fontBaseBold, sr, sg, sb);
    ty -= 30.0f;

    char nBuf[64];
    sprintf(nBuf, "Tamanho N: %d", g_nAtomic.load());
    PrintString(panelX + 15, ty, nBuf, fontBaseRegular, 1.0f, 1.0f, 1.0f);
    ty -= 18.0f;
    PrintString(panelX + 15, ty, "Teclas 1-9: 4, 6, 8, 10, 12, 16, 24, 32, 64", fontBaseRegular, 0.6f, 0.6f, 0.6f);
    ty -= 30.0f;

    sprintf(nBuf, "Passos: %lld", g_steps.load());
    PrintString(panelX + 15, ty, nBuf, fontBaseRegular, 0.9f, 0.9f, 0.9f);
    ty -= 18.0f;
    sprintf(nBuf, "Solucoes: %d", g_solutionsFound.load());
    PrintString(panelX + 15, ty, nBuf, fontBaseRegular, 0.9f, 0.9f, 0.9f);
    ty -= 30.0f;

    PrintString(panelX + 15, ty, "Velocidade (delay):", fontBaseRegular, 0.8f, 0.8f, 0.8f);
    ty -= 18.0f;
    sprintf(nBuf, "%d ms (SETAS CIMA/BAIXO)", g_delayMs.load());
    PrintString(panelX + 15, ty, nBuf, fontBaseBold, 1.0f, 1.0f, 0.4f);
    ty -= 45.0f;

    float btnW = panelW - 30.0f, btnH = 32.0f;
    bool isSolving = (g_status.load() == SOLVING);
    bool isPaused = (g_status.load() == PAUSED);

    const char* btnLabel = isPaused ? "Retomar" : (isSolving ? "Pausar" : "Iniciar (ESPACO)");
    if (DrawButton(panelX + 15, ty, btnW, btnH, btnLabel, isSolving, mx, my, clicked)) {
        if (g_status.load() == IDLE || g_status.load() == SOLVED) {
            StartSolver();
        } else if (g_status.load() == SOLVING) {
            g_status.store(PAUSED);
        } else if (g_status.load() == PAUSED) {
            g_status.store(SOLVING);
        }
    }
    ty -= 40.0f;

    if (DrawButton(panelX + 15, ty, btnW, btnH, "Reiniciar (R)", false, mx, my, clicked)) {
        StopSolver();
        ResetBoard(g_board, g_nAtomic.load());
        g_curRow.store(-1);
        g_curCol.store(-1);
        g_steps.store(0);
        g_solutionsFound.store(0);
        g_status.store(IDLE);
    }
    ty -= 40.0f;

    bool stepModeOn = g_stepMode.load();
    if (DrawButton(panelX + 15, ty, btnW, btnH, stepModeOn ? "Passo: ATIVO (P)" : "Passo: DESATIVADO (P)", stepModeOn, mx, my, clicked)) {
        g_stepMode.store(!stepModeOn);
        if (!stepModeOn) g_stepTrigger.store(true);
    }
    ty -= 40.0f;

    if (g_stepMode.load()) {
        if (DrawButton(panelX + 15, ty, btnW, btnH, "> Proximo Passo", false, mx, my, clicked)) {
            g_stepTrigger.store(true);
        }
        ty -= 40.0f;
    }

    ty -= 10.0f;
    PrintString(panelX + 15, ty, "Cursor (linha, coluna):", fontBaseRegular, 0.8f, 0.8f, 0.8f);
    ty -= 18.0f;
    int cr = g_curRow.load(), cc = g_curCol.load();
    sprintf(nBuf, "(%d, %d)", cr, cc);
    PrintString(panelX + 15, ty, nBuf, fontBaseBold, 1.0f, 0.8f, 0.3f);
    ty -= 30.0f;

    PrintString(panelX + 15, ty, "Pressione H para ajuda", fontBaseRegular, 0.5f, 0.5f, 0.5f);
}

int main(int argc, char* argv[]) {
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

    int nFromArgs = -1;
    if (argc >= 2) nFromArgs = std::atoi(argv[1]);

    if (nFromArgs > 0) {
        g_n = nFromArgs;
    } else {
        char iniPath[MAX_PATH] = {0};
        DWORD len = GetModuleFileNameA(NULL, iniPath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char* lastBackslash = std::strrchr(iniPath, '\\');
            if (lastBackslash) {
                *(lastBackslash + 1) = '\0';
                std::strcat(iniPath, "nQueen.ini");

                FILE* iniFile = std::fopen(iniPath, "r");
                if (iniFile) {
                    char line[256];
                    while (std::fgets(line, sizeof(line), iniFile)) {
                        if (std::strncmp(line, "N=", 2) == 0) {
                            int val = std::atoi(line + 2);
                            if (val > 0) {
                                g_n = val;
                            }
                            break;
                        }
                    }
                    std::fclose(iniFile);
                }
            }
        }
    }

    if (g_n < MIN_N) g_n = MIN_N;
    if (g_n > MAX_N) g_n = MAX_N;
    g_nAtomic.store(g_n);

    if (!CreateGLWindow("N Queen - Simulacao Visual com Backtracking", WINDOW_W, WINDOW_H)) {
        MessageBoxA(NULL, "Erro ao criar janela OpenGL.\nVerifique se os drivers de video estão instalados.", "Erro", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!g_hdc) {
        MessageBoxA(NULL, "Contexto de dispositivo (HDC) invalido.", "Erro OpenGL", MB_OK | MB_ICONERROR);
        return 1;
    }

    BuildFonts();

    bool running = true;
    bool showHelp = false;

    bool spaceDown = false;
    bool rDown = false;
    bool pDown = false;
    bool hDown = false;
    bool upDown = false;
    bool downDown = false;
    bool nDown[9] = {false};

    while (running) {
        ProcessMessages();

        if (!g_hwnd || !IsWindow(g_hwnd)) {
            running = false;
            break;
        }

        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(g_hwnd, &mousePos);
        int mx = mousePos.x;
        int my = WINDOW_H - mousePos.y;

        static bool lastMouseState = false;
        bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = currentMouseState && !lastMouseState;
        lastMouseState = currentMouseState;

        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spacePressed && !spaceDown) {
            if (g_status.load() == IDLE || g_status.load() == SOLVED) {
                StartSolver();
            } else if (g_status.load() == SOLVING) {
                g_status.store(PAUSED);
            } else if (g_status.load() == PAUSED) {
                g_status.store(SOLVING);
            }
        }
        spaceDown = spacePressed;

        bool rPressed = (GetAsyncKeyState('R') & 0x8000) != 0;
        if (rPressed && !rDown) {
            StopSolver();
            ResetBoard(g_board, g_nAtomic.load());
            g_curRow.store(-1);
            g_curCol.store(-1);
            g_steps.store(0);
            g_solutionsFound.store(0);
            g_status.store(IDLE);
        }
        rDown = rPressed;

        bool pPressed = (GetAsyncKeyState('P') & 0x8000) != 0;
        if (pPressed && !pDown) {
            bool cur = g_stepMode.load();
            g_stepMode.store(!cur);
            if (!cur) g_stepTrigger.store(true);
        }
        pDown = pPressed;

        bool hPressed = (GetAsyncKeyState('H') & 0x8000) != 0;
        if (hPressed && !hDown) {
            showHelp = !showHelp;
        }
        hDown = hPressed;

        bool upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
        if (upPressed && !upDown) {
            int cur = g_delayMs.load();
            g_delayMs.store(cur + 10);
        }
        upDown = upPressed;

        bool downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
        if (downPressed && !downDown) {
            int cur = g_delayMs.load();
            g_delayMs.store(std::max(0, cur - 10));
        }
        downDown = downPressed;

        auto trySetN = [&](int key, int val, bool& downRef) {
            bool pressed = (GetAsyncKeyState(key) & 0x8000) != 0;
            if (pressed && !downRef) {
                StopSolver();
                g_n = val;
                g_nAtomic.store(val);
                ResetBoard(g_board, val);
                g_curRow.store(-1);
                g_curCol.store(-1);
                g_steps.store(0);
                g_solutionsFound.store(0);
                g_status.store(IDLE);
            }
            downRef = pressed;
        };

        for (int i = 0; i < g_nOptionsCount; ++i) {
            trySetN('1' + i, g_nOptions[i], nDown[i]);
        }

        glClearColor(0.08f, 0.10f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, WINDOW_W, WINDOW_H);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WINDOW_W, 0, WINDOW_H, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        DrawRect(0, 0, (float)WINDOW_W, (float)WINDOW_H, 0.05f, 0.06f, 0.10f);

        PrintString(20, (float)(WINDOW_H - 30), "N QUEEN - SIMULACAO VISUAL", fontBaseLarge, 1.0f, 0.8f, 0.3f);
        PrintString(20, (float)(WINDOW_H - 45), "Backtracking com animacao em tempo real", fontBaseRegular, 0.8f, 0.8f, 0.8f);

        DrawBoard();

        DrawUI(mx, my, clicked, showHelp);

        if (showHelp) {
            DrawHelpCard((float)(WINDOW_W * 0.35f), (float)(WINDOW_H * 0.5f));
        }

        char titleBuffer[256];
        const char* st;
        switch (g_status.load()) {
            case IDLE:
                st = "Ocioso";
                break;
            case SOLVING:
                st = "Resolvendo...";
                break;
            case SOLVED:
                st = "SOLUCIONADO";
                break;
            case PAUSED:
                st = "Pausado";
                break;
        }
        sprintf(titleBuffer, "N Queen - N=%d | %s | Delay: %d ms | Passos: %lld | Solucoes: %d",
                g_nAtomic.load(), st, g_delayMs.load(), g_steps.load(), g_solutionsFound.load());
        SetWindowTextA(g_hwnd, titleBuffer);

        glFlush();
        SwapBuffers(g_hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    StopSolver();

    return 0;
}
