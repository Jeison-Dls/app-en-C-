#ifndef COLOREAR_H
#define COLOREAR_H

#include <string>

// Códigos de colores ANSI
const std::string RESET = "\033[0m";
const std::string ROJO = "\033[31m";
const std::string VERDE = "\033[32m";
const std::string AMARILLO = "\033[33m";
const std::string AZUL = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CIAN = "\033[36m";
const std::string BLANCO = "\033[37m";

// Función para aplicar colores
std::string colorear(const std::string& texto, const std::string& color);

#endif // COLOREAR_H
