#include <windows.h>
#include "Window.h"

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int mostrar) {
    Window janela;

    if (!janela.criar(instancia, mostrar)) {
        MessageBoxA(nullptr, "Erro ao criar a janela.", "HashDB", MB_ICONERROR);
        return 1;
    }

    janela.executar();
    return 0;
}
