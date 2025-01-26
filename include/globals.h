#ifndef GLOBALS_H
#define GLOBALS_H

#include <mutex>
#include <atomic>

extern std::atomic<bool> registroExitoso;
extern std::mutex mtx; // Declaración de la variable global

#endif
