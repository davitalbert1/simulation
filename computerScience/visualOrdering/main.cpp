#include "window.h"
#include <gl/gl.h>
#include <windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <string>
#include <cstring>

const int NUM_ELEMENTS = 100;
int arr[NUM_ELEMENTS];
bool isSorted[NUM_ELEMENTS];

std::atomic<int> compareIndex1{-1};
std::atomic<int> compareIndex2{-1};
std::atomic<int> delayMs{15};
std::atomic<bool> stopSorting{false};
std::atomic<bool> sortingActive{false};
const char* currentAlgoName = "Nenhum";

std::thread sortThread;

static GLuint fontBaseRegular = 0;
static GLuint fontBaseBold = 0;
static GLuint fontBaseLarge = 0;
static bool showHelpCard = true;

// O HUD utiliza uma projeção paralela ortográfica bidimensional (2D) para a renderização de elementos textuais e painéis sobre a viewport.
// As fontes tipográficas são geradas por bitmaps a partir da API Win32 do Windows.
static void BuildFonts() {
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
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base - 32);
    glCallLists((GLsizei)std::strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
}

static void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawRectOutline(float x, float y, float w, float h, float r, float g, float b, float thickness = 1.0f) {
    glColor3f(r, g, b);
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
    
    float r = 0.12f, g = 0.14f, b = 0.22f;
    if (active) {
        r = 0.16f; g = 0.40f; b = 0.90f;
    } else if (hovered) {
        r = 0.20f; g = 0.23f; b = 0.35f;
    }
    
    DrawRect(x, y, w, h, r, g, b, 0.85f);
    DrawRectOutline(x, y, w, h, 0.3f, 0.4f, 0.6f);
    
    int labelLen = (int)std::strlen(label);
    float textW = labelLen * 7.0f;
    float tx = x + (w - textW) / 2.0f;
    float ty = y + (h - 10.0f) / 2.0f;
    
    PrintString(tx, ty, label, fontBaseBold, 1.0f, 1.0f, 1.0f);
    
    return hovered && clicked;
}

static void DrawHelpCardHUD(float cx, float cy) {
    float w = 400.0f;
    float h = 330.0f;
    float cardX = cx - w / 2.0f;
    float cardY = cy - h / 2.0f;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawRect(cardX, cardY, w, h, 0.05f, 0.06f, 0.11f, 0.92f);
    DrawRectOutline(cardX, cardY, w, h, 0.3f, 0.6f, 0.9f, 2.0f);
    glDisable(GL_BLEND);
    
    float tx = cardX + 20.0f;
    float ty = cardY + h - 30.0f;
    PrintString(tx + 60.0f, ty, "CONTROLES DE ORDENACAO", fontBaseLarge, 0.3f, 0.7f, 1.0f);
    ty -= 30.0f;
    
    PrintString(tx, ty, "[ Executar Algoritmos ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- B : Iniciar Bubble Sort.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 32.0f, "- S : Iniciar Selection Sort.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 48.0f, "- I : Iniciar Insertion Sort.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 64.0f, "- Q : Iniciar Quick Sort.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 80.0f, "- M : Iniciar Merge Sort.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    ty -= 100.0f;
    
    PrintString(tx, ty, "[ Ajustes e Acoes ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- R : Embaralhar os elementos.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    PrintString(tx + 15.0f, ty - 32.0f, "- Setas UP / DOWN : Modificar atraso / velocidade.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
    ty -= 50.0f;
    
    PrintString(tx, ty, "[ Atalhos ]", fontBaseBold, 0.9f, 0.9f, 1.0f);
    PrintString(tx + 15.0f, ty - 16.0f, "- H : Ocultar / Mostrar este painel de ajuda.", fontBaseRegular, 0.8f, 0.8f, 0.9f);
}

bool wait() {
    int d = delayMs.load();
    for (int i = 0; i < d; ++i) {
        if (stopSorting.load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (d == 0) std::this_thread::yield();
    return !stopSorting.load();
}

// Bubble Sort: Algoritmo de ordenacao quadratico O(n^2) baseado na comparacao repetitiva de elementos adjacentes e troca se estiverem fora de ordem.
void bubbleSort() {
    for (int i = 0; i < NUM_ELEMENTS - 1; ++i) {
        for (int j = 0; j < NUM_ELEMENTS - i - 1; ++j) {
            compareIndex1.store(j);
            compareIndex2.store(j + 1);
            if (!wait()) return;

            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                if (!wait()) return;
            }
        }
        isSorted[NUM_ELEMENTS - i - 1] = true;
    }
    isSorted[0] = true;
}

// Selection Sort: Algoritmo quadratico O(n^2) que busca iterativamente o menor elemento da particao nao ordenada e o posiciona no inicio.
void selectionSort() {
    for (int i = 0; i < NUM_ELEMENTS - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < NUM_ELEMENTS; ++j) {
            compareIndex1.store(j);
            compareIndex2.store(minIdx);
            if (!wait()) return;

            if (arr[j] < arr[minIdx]) minIdx = j;
        }
        if (minIdx != i) {
            std::swap(arr[i], arr[minIdx]);
            if (!wait()) return;
        }
        isSorted[i] = true;
    }
    isSorted[NUM_ELEMENTS - 1] = true;
}

// Insertion Sort: Algoritmo quadratico O(n^2) que insere de forma incremental cada elemento na sua posicao correta na sublista ja ordenada.
void insertionSort() {
    isSorted[0] = true;
    for (int i = 1; i < NUM_ELEMENTS; ++i) {
        int key = arr[i];
        int j = i - 1;
        compareIndex1.store(i);
        if (!wait()) return;

        while (j >= 0 && arr[j] > key) {
            compareIndex1.store(j);
            compareIndex2.store(j + 1);
            arr[j + 1] = arr[j];
            if (!wait()) return;
            j = j - 1;
        }
        arr[j + 1] = key;
        compareIndex1.store(-1);
        compareIndex2.store(-1);
        if (!wait()) return;

        for (int k = 0; k <= i; ++k) isSorted[k] = true;
    }
}

int partition(int low, int high) {
    int pivot = arr[high];
    int i = (low - 1);
    for (int j = low; j < high; ++j) {
        compareIndex1.store(j);
        compareIndex2.store(high);
        if (!wait()) return -1;

        if (arr[j] < pivot) {
            i++;
            std::swap(arr[i], arr[j]);
            if (!wait()) return -1;
        }
    }
    std::swap(arr[i + 1], arr[high]);
    if (!wait()) return -1;
    return (i + 1);
}

void quickSortHelper(int low, int high) {
    if (low < high) {
        int pi = partition(low, high);
        if (pi == -1 || stopSorting.load()) return;
        
        quickSortHelper(low, pi - 1);
        if (stopSorting.load()) return;
        
        isSorted[pi] = true;
        
        quickSortHelper(pi + 1, high);
    } else if (low == high) {
        isSorted[low] = true;
    }
}

// Quick Sort: Algoritmo de divisao e conquista com complexidade media O(n log n), baseado no particionamento do array em torno de um elemento pivo.
void quickSort() {
    quickSortHelper(0, NUM_ELEMENTS - 1);
    for (int i = 0; i < NUM_ELEMENTS; ++i) isSorted[i] = true;
}

void merge(int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    std::vector<int> L(n1), R(n2);
    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        compareIndex1.store(l + i);
        compareIndex2.store(m + 1 + j);
        if (!wait()) return;

        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
        if (!wait()) return;
    }

    while (i < n1) {
        compareIndex1.store(l + i);
        if (!wait()) return;
        arr[k] = L[i];
        i++;
        k++;
        if (!wait()) return;
    }

    while (j < n2) {
        compareIndex2.store(m + 1 + j);
        if (!wait()) return;
        arr[k] = R[j];
        j++;
        k++;
        if (!wait()) return;
    }
}

void mergeSortHelper(int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSortHelper(l, m);
        if (stopSorting.load()) return;
        mergeSortHelper(m + 1, r);
        if (stopSorting.load()) return;
        merge(l, m, r);
    }
}

// Merge Sort: Algoritmo estavel de divisao e conquista com complexidade O(n log n), que divide recursivamente o array e faz a intercalacao (merge).
void mergeSort() {
    mergeSortHelper(0, NUM_ELEMENTS - 1);
    for (int i = 0; i < NUM_ELEMENTS; ++i) isSorted[i] = true;
}

void runSort(void (*sortFunc)()) {
    sortingActive.store(true);
    sortFunc();
    compareIndex1.store(-1);
    compareIndex2.store(-1);
    sortingActive.store(false);
}

void startSortingThread(void (*sortFunc)(), const char* name) {
    stopSorting.store(true);
    if (sortThread.joinable()) sortThread.join();
    
    for (int i = 0; i < NUM_ELEMENTS; ++i) isSorted[i] = false;
    
    stopSorting.store(false);
    currentAlgoName = name;
    sortThread = std::thread(runSort, sortFunc);
}

void shuffleArray() {
    stopSorting.store(true);
    if (sortThread.joinable()) sortThread.join();
    
    currentAlgoName = "Nenhum (Pressione B, S, I, Q, M para iniciar)";
    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        arr[i] = i + 1;
        isSorted[i] = false;
    }
    for (int i = NUM_ELEMENTS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        std::swap(arr[i], arr[j]);
    }
    compareIndex1.store(-1);
    compareIndex2.store(-1);
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    if (!CreateGLWindow("Ordenacao Visual", 800, 600)) {
        return 1;
    }

    BuildFonts();
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glDisable(GL_DEPTH_TEST); // Apenas renderização 2D

    shuffleArray();

    bool bKeyDown = false;
    bool sKeyDown = false;
    bool iKeyDown = false;
    bool qKeyDown = false;
    bool mKeyDown = false;
    bool rKeyDown = false;
    bool upKeyDown = false;
    bool downKeyDown = false;
    bool hKeyDown = false;

    while (true) {
        ProcessMessages();

        // Leitura de teclado
        if (GetAsyncKeyState('B')) {
            if (!bKeyDown && !sortingActive.load()) {
                startSortingThread(bubbleSort, "Bubble Sort");
                bKeyDown = true;
            }
        } else {
            bKeyDown = false;
        }

        if (GetAsyncKeyState('S')) {
            if (!sKeyDown && !sortingActive.load()) {
                startSortingThread(selectionSort, "Selection Sort");
                sKeyDown = true;
            }
        } else {
            sKeyDown = false;
        }

        if (GetAsyncKeyState('I')) {
            if (!iKeyDown && !sortingActive.load()) {
                startSortingThread(insertionSort, "Insertion Sort");
                iKeyDown = true;
            }
        } else {
            iKeyDown = false;
        }

        if (GetAsyncKeyState('Q')) {
            if (!qKeyDown && !sortingActive.load()) {
                startSortingThread(quickSort, "Quick Sort");
                qKeyDown = true;
            }
        } else {
            qKeyDown = false;
        }

        if (GetAsyncKeyState('M')) {
            if (!mKeyDown && !sortingActive.load()) {
                startSortingThread(mergeSort, "Merge Sort");
                mKeyDown = true;
            }
        } else {
            mKeyDown = false;
        }

        if (GetAsyncKeyState('R')) {
            if (!rKeyDown) {
                shuffleArray();
                rKeyDown = true;
            }
        } else {
            rKeyDown = false;
        }

        if (GetAsyncKeyState(VK_UP)) {
            if (!upKeyDown) {
                delayMs.store(delayMs.load() + 5);
                upKeyDown = true;
            }
        } else {
            upKeyDown = false;
        }

        if (GetAsyncKeyState(VK_DOWN)) {
            if (!downKeyDown) {
                int current = delayMs.load();
                if (current > 0) delayMs.store(current - 5 > 0 ? current - 5 : 0);
                downKeyDown = true;
            }
        } else {
            downKeyDown = false;
        }

        bool hPressed = (GetAsyncKeyState('H') & 0x8000) != 0;
        if (hPressed && !hKeyDown) {
            showHelpCard = !showHelpCard;
        }
        hKeyDown = hPressed;

        // Renderização dos elementos do vetor
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0, 0, 800, 600);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 800, 0, 600, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float barWidth = 800.0f / NUM_ELEMENTS;
        for (int i = 0; i < NUM_ELEMENTS; ++i) {
            float x1 = i * barWidth;
            float x2 = (i + 1) * barWidth - 1.0f; // Espaçamento de 1px
            float y1 = 20.0f;
            float y2 = y1 + (arr[i] / static_cast<float>(NUM_ELEMENTS)) * 540.0f;

            if (i == compareIndex1.load() || i == compareIndex2.load()) {
                glColor3f(1.0f, 0.2f, 0.2f); // Cor vermelha representa comparação ativa entre índices.
            } else if (isSorted[i]) {
                glColor3f(0.2f, 0.8f, 0.2f); // Cor verde indica elementos posicionados e ordenados.
            } else {
                glColor3f(0.2f, 0.6f, 1.0f); // Cor azul ciano para elementos não ordenados.
            }

            glBegin(GL_QUADS);
            glVertex2f(x1, y1);
            glVertex2f(x2, y1);
            glVertex2f(x2, y2);
            glVertex2f(x1, y2);
            glEnd();
        }

        // --- RENDERIZAR INTERFACE HUD (2D ORTOGRÁFICA) ---
        POINT mousePos;
        GetCursorPos(&mousePos);
        ScreenToClient(g_hwnd, &mousePos);
        int mx = mousePos.x;
        int my = 600 - mousePos.y;

        static bool lastMouseState = false;
        bool currentMouseState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = currentMouseState && !lastMouseState;
        lastMouseState = currentMouseState;

        // Botão para exibir ou fechar o card de ajuda
        if (DrawButton(20, 548, 120, 32, showHelpCard ? "Fechar Ajuda" : "Ajuda (H)", false, mx, my, clicked)) {
            showHelpCard = !showHelpCard;
        }

        if (showHelpCard) {
            DrawHelpCardHUD(400.0f, 300.0f);
        }
        // -------------------------------------------------

        // Atualizar barra de título da janela
        char titleBuffer[256];
        sprintf(titleBuffer, "Ordenacao Visual - [%s] [Atraso: %d ms] - B: Bubble, S: Selection, I: Insertion, Q: Quick, M: Merge, R: Shuffle, UP/DOWN: Atraso", 
                currentAlgoName, delayMs.load());
        SetWindowText(g_hwnd, titleBuffer);

        SwapBuffers(g_hdc);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Parar thread se estiver rodando
    stopSorting.store(true);
    if (sortThread.joinable()) sortThread.join();

    return 0;
}
