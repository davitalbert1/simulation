#include "Window.h"
#include "Hash.h"
#include "Database.h"
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

static std::string obterCaminhoDatabase() {
    char caminhoExe[MAX_PATH];
    DWORD tamanho = GetModuleFileNameA(nullptr, caminhoExe, MAX_PATH);
    if (tamanho == 0) return "database.txt";
    std::string caminho(caminhoExe, tamanho);
    size_t ultimaBarra = caminho.find_last_of("\\/");
    if (ultimaBarra == std::string::npos) return "database.txt";
    return caminho.substr(0, ultimaBarra + 1) + "database.txt";
}

Window::Window() {
    hwnd = nullptr;
    campoArquivo = nullptr;
    campoHash = nullptr;
    botaoGerar = nullptr;
    botaoBuscar = nullptr;
    resultado = nullptr;
}

bool Window::criar(HINSTANCE instancia, int mostrar) {
    const char* CLASS_NAME = "HashDBWindow";

    WNDCLASSA wc{};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instancia;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassA(&wc);

    hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "HashDB - Banco de Dados Educacional",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        700,
        450,
        nullptr,
        nullptr,
        instancia,
        this
    );

    if (!hwnd) return false;

    ShowWindow(hwnd, mostrar);
    UpdateWindow(hwnd);

    return true;
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT mensagem, WPARAM wParam, LPARAM lParam) {
    Window* janela = nullptr;

    if (mensagem == WM_NCCREATE) {
        CREATESTRUCTA* dados = reinterpret_cast<CREATESTRUCTA*>(lParam);
        janela = reinterpret_cast<Window*>(dados->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(janela));
        janela->hwnd = hwnd;
    } else {
        janela = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    if (janela) return janela->tratarMensagem(mensagem, wParam, lParam);

    return DefWindowProcA(hwnd, mensagem, wParam, lParam);
}

LRESULT Window::tratarMensagem(UINT mensagem, WPARAM wParam, LPARAM lParam) {
    switch (mensagem) {
        case WM_CREATE: {
            CreateWindowA(
                "STATIC",
                "Arquivo:",
                WS_VISIBLE | WS_CHILD,
                20,
                25,
                100,
                25,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );
            campoArquivo = CreateWindowA(
                "EDIT",
                "",
                WS_VISIBLE |
                WS_CHILD |
                WS_BORDER |
                ES_AUTOHSCROLL,
                100,
                20,
                430,
                30,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );
            botaoGerar = CreateWindowA(
                "BUTTON",
                "Gerar Hash",
                WS_VISIBLE | WS_CHILD,
                540,
                20,
                120,
                30,
                hwnd,
                reinterpret_cast<HMENU>(1),
                nullptr,
                nullptr
            );
            CreateWindowA(
                "STATIC",
                "Hash:",
                WS_VISIBLE |
                WS_CHILD,
                20,
                80,
                100,
                25,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );
            campoHash = CreateWindowA(
                "EDIT",
                "",
                WS_VISIBLE |
                WS_CHILD |
                WS_BORDER |
                ES_AUTOHSCROLL,
                100,
                75,
                430,
                30,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );
            botaoBuscar = CreateWindowA(
                "BUTTON",
                "Buscar",
                WS_VISIBLE | WS_CHILD,
                540,
                75,
                120,
                30,
                hwnd,
                reinterpret_cast<HMENU>(2),
                nullptr,
                nullptr
            );
            resultado = CreateWindowA(
                "EDIT",
                "Resultado aparecerá aqui...",
                WS_VISIBLE |
                WS_CHILD |
                WS_BORDER |
                ES_MULTILINE |
                ES_AUTOVSCROLL |
                ES_READONLY,
                20,
                130,
                640,
                240,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );

            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 1) {
                gerarHash();
            } else if (id == 2) {
                procurarHash();
            }

            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, mensagem, wParam, lParam);
}

void Window::gerarHash() {
    char buffer[1024];

    GetWindowTextA(campoArquivo, buffer, sizeof(buffer));

    std::string caminho = buffer;

    if (caminho.empty()) {
        SetWindowTextA(resultado, "Digite o caminho do arquivo.");
        return;
    }

    std::ifstream arquivo(caminho, std::ios::binary);

    if (!arquivo.is_open()) {
        SetWindowTextA(resultado, "Nao foi possivel abrir o arquivo.");
        return;
    }

    std::ostringstream conteudo;

    conteudo << arquivo.rdbuf();

    std::string dados = conteudo.str();
    std::string hash = calcularHash(dados);

    SetWindowTextA(campoHash, hash.c_str());

    Database banco;
    std::string database = obterCaminhoDatabase();
    banco.carregar(database);
    banco.adicionar(hash, caminho);
    banco.salvar(database);

    std::string mensagem = "Hash criado:\r\n\r\n" +
        hash + "\r\n\r\n" +
        "Arquivo:\r\n" + caminho + "\r\n\r\n" +
        "Registros no indice: " + std::to_string(banco.tamanho());

    SetWindowTextA(resultado, mensagem.c_str());
}

void Window::procurarHash() {
    char buffer[256];

    GetWindowTextA(campoHash, buffer, sizeof(buffer));

    std::string hash = buffer;

    if (hash.empty()) {
        SetWindowTextA(resultado, "Digite um hash para procurar.");
        return;
    }

    Database banco;

    std::string database = obterCaminhoDatabase();

    if (!banco.carregar(database)) {
        SetWindowTextA(resultado, "O banco de dados ainda nao existe.");
        return;
    }

    std::string caminho;

    if (!banco.procurar(hash, caminho)) {
        SetWindowTextA(resultado, "Hash nao encontrado.");
        return;
    }

    std::string mensagem = "Hash encontrado!\r\n\r\n"
        "Hash:\r\n" + hash + "\r\n\r\n" "Arquivo associado:\r\n" + caminho;

    SetWindowTextA(resultado, mensagem.c_str());
}

void Window::executar() {
    MSG mensagem{};

    while (GetMessageA(&mensagem, nullptr, 0, 0)) {
        TranslateMessage(&mensagem);
        DispatchMessageA(&mensagem);
    }
}
