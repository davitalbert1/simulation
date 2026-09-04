#pragma once
#include <windows.h>

class Window {
private:
    HWND hwnd;
    HWND campoArquivo;
    HWND campoHash;
    HWND botaoGerar;
    HWND botaoBuscar;
    HWND resultado;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT mensagem, WPARAM wParam, LPARAM lParam);

    LRESULT tratarMensagem(UINT mensagem, WPARAM wParam, LPARAM lParam);

    void gerarHash();

    void procurarHash();

public:
    Window();

    bool criar(HINSTANCE instancia, int mostrar);

    void executar();
};
