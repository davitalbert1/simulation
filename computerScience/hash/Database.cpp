#include "Database.h"
#include <fstream>

bool Database::carregar(const std::string& arquivo) {
    indice.clear();
    std::ifstream entrada(arquivo);
    if (!entrada.is_open()) return false;
    std::string linha;

    while (std::getline(entrada, linha)) {
        if (linha.empty()) continue;
        size_t separador = linha.find('|');
        if (separador == std::string::npos) continue;
        std::string hash = linha.substr(0, separador);
        std::string caminho = linha.substr(separador + 1);
        indice[hash] = caminho;
    }

    return true;
}

bool Database::salvar(const std::string& arquivo) const {
    std::ofstream saida(arquivo);
    if (!saida.is_open()) return false;
    for (const auto& item : indice) saida << item.first << "|" << item.second << "\n";
    return true;
}

void Database::adicionar(const std::string& hash, const std::string& caminho) {
    indice[hash] = caminho;
}

bool Database::procurar(const std::string& hash, std::string& caminho) const {
    auto resultado = indice.find(hash);
    if (resultado == indice.end()) return false;
    caminho = resultado->second;
    return true;
}

size_t Database::tamanho() const {
    return indice.size();
}
