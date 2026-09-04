#include "Hash.h"
#include <sstream>
#include <iomanip>

// Rotação de bits para a esquerda.
static uint32_t rotl32(uint32_t valor, int quantidade) {
    return (valor << quantidade) | (valor >> (32 - quantidade));
}

std::string calcularHash(const std::string& dados) {
    // Quatro estados internos.
    uint32_t h1 = 0x12345678;
    uint32_t h2 = 0x9ABCDEF0;
    uint32_t h3 = 0x0F1E2D3C;
    uint32_t h4 = 0x4B5A6978;

    // Processa cada byte do arquivo.
    for (unsigned char c : dados) {
        // Mistura o byte atual no primeiro estado.
        h1 ^= c;

        h1 *= 0x045D9F3B;

        h1 = rotl32(h1, 5);

        // Propaga a informação entre os estados.
        h2 ^= h1;
        h2 *= 0x27D4EB2D;
        h2 = rotl32(h2, 7);

        h3 += h2 ^ c;
        h3 *= 0x165667B1;
        h3 = rotl32(h3, 11);

        h4 ^= h3 + 0x9E3779B9;
        h4 *= 0x85EBCA6B;
        h4 = rotl32(h4, 13);
    }

    // Mistura final.
    h1 ^= h2 >> 16;
    h1 *= 0x7FEB352D;
    h1 ^= h1 >> 15;

    h2 ^= h3 >> 13;
    h2 *= 0x846CA68B;
    h2 ^= h2 >> 16;

    h3 ^= h4 >> 16;
    h3 *= 0x7FEB352D;
    h3 ^= h3 >> 15;

    h4 ^= h1 >> 13;
    h4 *= 0x846CA68B;
    h4 ^= h4 >> 16;

    // Converte os quatro estados para hexadecimal.
    std::ostringstream resultado;

    resultado
        << std::hex
        << std::setfill('0')
        << std::setw(8) << h1
        << std::setw(8) << h2
        << std::setw(8) << h3
        << std::setw(8) << h4;

    return resultado.str();
}
