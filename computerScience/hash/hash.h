#pragma once
#include <string>
#include <cstdint>

// Recebe os bytes do arquivo e produz uma representação hexadecimal.
std::string calcularHash(const std::string& dados);
