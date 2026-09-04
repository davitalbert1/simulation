#pragma once
#include <string>
#include <unordered_map>

class Database {
private:
    std::unordered_map<std::string, std::string> indice;

public:
    bool carregar(const std::string& arquivo);
    bool salvar(const std::string& arquivo) const;
    void adicionar(const std::string& hash, const std::string& caminho);
    bool procurar(const std::string& hash, std::string& caminho) const;
    size_t tamanho() const;
};
