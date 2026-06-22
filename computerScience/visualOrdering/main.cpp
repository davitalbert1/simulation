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

bool wait() {
    int d = delayMs.load();
    for (int i = 0; i < d; ++i) {
        if (stopSorting.load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (d == 0) std::this_thread::yield();
    return !stopSorting.load();
}

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
    CreateGLWindow("Ordenacao Visual", 800, 600);

    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glDisable(GL_DEPTH_TEST); // 2D rendering only

    shuffleArray();

    bool bKeyDown = false;
    bool sKeyDown = false;
    bool iKeyDown = false;
    bool qKeyDown = false;
    bool mKeyDown = false;
    bool rKeyDown = false;
    bool upKeyDown = false;
    bool downKeyDown = false;

    while (true) {
        ProcessMessages();

        // Keyboard inputs
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

        // Render
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
            float x2 = (i + 1) * barWidth - 1.0f; // 1px separation
            float y1 = 20.0f;
            float y2 = y1 + (arr[i] / static_cast<float>(NUM_ELEMENTS)) * 540.0f;

            if (i == compareIndex1.load() || i == compareIndex2.load()) {
                glColor3f(1.0f, 0.2f, 0.2f); // Red for comparisons
            } else if (isSorted[i]) {
                glColor3f(0.2f, 0.8f, 0.2f); // Green for sorted
            } else {
                glColor3f(0.2f, 0.6f, 1.0f); // Cyan for default
            }

            glBegin(GL_QUADS);
            glVertex2f(x1, y1);
            glVertex2f(x2, y1);
            glVertex2f(x2, y2);
            glVertex2f(x1, y2);
            glEnd();
        }

        // Update Title Bar
        char titleBuffer[256];
        sprintf(titleBuffer, "Ordenacao Visual - [%s] [Atraso: %d ms] - B: Bubble, S: Selection, I: Insertion, Q: Quick, M: Merge, R: Shuffle, UP/DOWN: Atraso", 
                currentAlgoName, delayMs.load());
        SetWindowText(g_hwnd, titleBuffer);

        SwapBuffers(g_hdc);
        
        // Cap frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup thread if active
    stopSorting.store(true);
    if (sortThread.joinable()) sortThread.join();

    return 0;
}
