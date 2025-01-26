#include "colorear.h"

std::string colorear(const std::string& texto, const std::string& color) {
    return color + texto + "\033[0m"; // Agrega RESET al final para volver al color predeterminado
}
