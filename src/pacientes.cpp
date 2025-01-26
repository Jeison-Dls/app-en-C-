#include <iostream>
#include <string>
#include <sqlite3.h>
#include <thread>
#include <mutex>
#include "globals.h"
#include "pacientes.h"
#include "globals.h"
#include <atomic> // Incluir para usar std::atomic
#include "colorear.h"// Archivo que contiene la funcion `colorear`


std::atomic<bool> registroExitoso(false);

// Funcion para consultar la tabla de pacientes antes del registro
void consultarPacientesAntesRegistro(sqlite3* db) {
    std::unique_lock<std::mutex> lock(mtx); // Bloqueo para sincronización

    const char* sqlCheck = "SELECT COUNT(*) FROM pacientes;"; // Verificar si hay pacientes registrados
    sqlite3_stmt* stmt;

    // Preparar y ejecutar consulta para contar pacientes
    if (sqlite3_prepare_v2(db, sqlCheck, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al verificar la tabla de pacientes: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
        // Mostrar la consulta con el nombre del medico asignado
        const char* sql = R"(
            SELECT p.id, p.nombre, p.edad, p.genero, p.telefono, p.email, 
                   p.prioridad, 
                   CASE 
                       WHEN p.medico_asignado IS NULL OR p.medico_asignado = 'Sin asignar' THEN 'Sin asignar'
                       ELSE (SELECT m.nombre FROM medicos m WHERE m.id = p.medico_asignado)
                   END AS medico_asignado_nombre
            FROM pacientes p;
        )";
        sqlite3_stmt* stmtConsulta;

        if (sqlite3_prepare_v2(db, sql, -1, &stmtConsulta, 0) != SQLITE_OK) {
            std::cerr << colorear("Error al preparar la consulta para pacientes: ", ROJO) << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt); // Liberar recursos antes de salir
            return;
        }

        std::cout << colorear("\n=== Lista de Pacientes Registrados ===\n", AZUL);
        while (sqlite3_step(stmtConsulta) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmtConsulta, 0);
            std::string nombre = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 1));
            int edad = sqlite3_column_int(stmtConsulta, 2);
            std::string genero = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 3));
            std::string telefono = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 4));
            std::string email = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 5));
            //std::string prioridad = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 6));
            std::string prioridad;
            const char* prioridadTexto = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 6));

            if (prioridadTexto == nullptr) {
                prioridad = "Sin prioridad"; // Valor por defecto si el texto es NULL
            } else {
                prioridad = prioridadTexto;
            }
            // Manejo seguro de valores NULL en la columna medico asignado
            std::string medicoAsignadoNombre;
            const char* medicoAsignadoTexto = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 7));
            if (medicoAsignadoTexto == nullptr) {
                medicoAsignadoNombre = "Sin asignar";
            } else {
                medicoAsignadoNombre = medicoAsignadoTexto;
            }

            

            std::cout << colorear("ID: ", VERDE) << id << "\n"
                      << colorear("Nombre: ", VERDE) << nombre << "\n"
                      << colorear("Edad: ", VERDE) << edad << "\n"
                      << colorear("Genero: ", VERDE) << genero << "\n"
                      << colorear("Telefono: ", VERDE) << telefono << "\n"
                      << colorear("Email: ", VERDE) << email << "\n"
                      << colorear("Prioridad: ", VERDE) << prioridad << "\n"
                      << colorear("Medico Asignado: ", VERDE) << medicoAsignadoNombre << "\n\n";
        }
        sqlite3_finalize(stmtConsulta);

    } else {
        std::cout << colorear("\nNo hay pacientes registrados actualmente.\n", ROJO);
    }

    sqlite3_finalize(stmt);
}

// Funcion para validar los datos del paciente
bool validarDatosPaciente(const std::string& nombre, int edad, const std::string& genero, const std::string& telefono, const std::string& email) {
    if (nombre.empty() || genero.empty() || telefono.empty() || email.empty() || (genero != "F" && genero != "M")) {
        std::cerr << colorear("Error: Todos los campos son obligatorios y el genero debe ser 'F' o 'M'.\n", ROJO);
        return false;
    }
    if (edad <= 0) {
        std::cerr << colorear("Error: La edad debe ser un numero positivo.\n", ROJO);
        return false;
    }
    return true;
}

// Funcion para registrar un paciente
void registrarPaciente(sqlite3* db, int& pacienteId) {
    std::unique_lock<std::mutex> lock(mtx); // Bloqueo para sincronización

    std::string nombre, genero, telefono, email;
    int edad;
    pacienteId = -1;  // Inicializar el pacienteId por si el registro falla

    // Solicitar datos del paciente
    std::cout << colorear("Ingrese el nombre completo del paciente: ", AMARILLO);
    std::getline(std::cin, nombre);

    std::cout << colorear("Ingrese la edad del paciente: ", AMARILLO);
    std::cin >> edad;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << colorear("Ingrese el genero (F/M): ", AMARILLO);
    std::getline(std::cin, genero);

    std::cout << colorear("Ingrese el telefono: ", AMARILLO);
    std::getline(std::cin, telefono);

    std::cout << colorear("Ingrese el email: ", AMARILLO);
    std::getline(std::cin, email);

    // Validar los datos antes de registrar
    if (!validarDatosPaciente(nombre, edad, genero, telefono, email)) {
        registroExitoso = false;  // Indicar que el registro fallo
        return;
    }

    // Inicializamos la prioridad como "Pendiente"
    std::string prioridad = "Pendiente";

    const char* sql = "INSERT INTO pacientes (nombre, edad, genero, telefono, email, prioridad) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        registroExitoso = false;
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, edad);
    sqlite3_bind_text(stmt, 3, genero.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, telefono.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, prioridad.c_str(), -1, SQLITE_STATIC);  // Guardamos prioridad inicial como "Pendiente"

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << colorear("Error al registrar al paciente: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        registroExitoso = false;
    } else {
        registroExitoso = true;
        pacienteId = sqlite3_last_insert_rowid(db);  // Obtener el ID del paciente recien registrado
        std::cout << colorear("Paciente registrado correctamente. Prioridad inicial: ", VERDE) << prioridad << "\n";
    }

    sqlite3_finalize(stmt); // Liberar recursos
}


// Funcion para asignar prioridad automáticamente
void asignarPrioridad(sqlite3* db, int pacienteId) {
    while (!registroExitoso) {
        // Esperar hasta que el registro sea exitoso
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Espera hasta que el registro sea exitoso
    }

    if (!registroExitoso) {
        // Si el registro falla, cancelar la asignación
        std::cout << colorear("La asignacion de prioridad fue cancelada.\n", ROJO);
        return;
    }

    std::unique_lock<std::mutex> lock(mtx); // Bloqueo para sincronización

    // Consultar la edad del paciente
    const char* sql = "SELECT edad FROM pacientes WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear ("Error al preparar la consulta para asignar prioridad: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, pacienteId);

    int edad = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        edad = sqlite3_column_int(stmt, 0);
    } else {
        std::cerr << colorear("No se encontro el paciente con ID ", ROJO) << pacienteId << ".\n";
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // Asignar prioridad basada en la edad
    std::string prioridad = (edad > 60) ? "Alta" : "Baja";

    const char* updateSql = "UPDATE pacientes SET prioridad = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para actualizar prioridad: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, prioridad.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, pacienteId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << colorear("Error al asignar la prioridad: ", ROJO) << sqlite3_errmsg(db) << std::endl;
    } else {
        std::cout << colorear("Prioridad asignada automáticamente: ", VERDE) << prioridad << "\n";
    }

    sqlite3_finalize(stmt);
}




// Menu para pacientes
void menuPacientes(sqlite3* db) {
    int pacienteId = -1; // Almacena el ID del paciente registrado

    // Crear hilos para la consulta y el registro
    std::thread consulta([&]() { consultarPacientesAntesRegistro(db); }); // Hilo para consultar pacientes existentes
    std::thread registro([&]() { registrarPaciente(db, pacienteId); }); // Hilo para registrar un nuevo paciente
    std::thread prioridad; // Hilo para asignar prioridad (inicializado vacío)

    // Esperar a que el registro termine para iniciar la asignacion de prioridad
    registro.join(); // Unir el hilo de registro para asegurarse de que el registro termine antes de asignar prioridad

    if (registroExitoso) {
        // Si el registro fue exitoso, crear un hilo para asignar prioridad automáticamente
        prioridad = std::thread([&]() { asignarPrioridad(db, pacienteId); });
    }

    // Unir hilos restantes
    consulta.join(); // Unir el hilo de consulta para esperar a que termine
    if (prioridad.joinable()) {
        prioridad.join(); // Unir el hilo de asignación de prioridad si fue creado
    }

    // Verificar si el registro fue exitoso
    if (!registroExitoso) {
        std::cout << colorear("El registro del paciente fallo. Intente nuevamente.\n", ROJO);
    }
}

/*
Resumen del paralelismo en este codigo
Consulta y registro en paralelo:
consultarPacientesAntesRegistro y registrarPaciente se ejecutan simultáneamente en hilos separados. Esto reduce el tiempo de espera al realizar ambas operaciones al mismo tiempo.

Asignación de prioridad condicionada:
Un tercer hilo ejecuta asignarPrioridad solo si el registro fue exitoso (registroExitoso == true).

Sincronización:
std::mutex y std::unique_lock protegen las operaciones críticas y evitar condiciones de carrera.
Utilizas std::atomic<bool> para manejar el estado de éxito del registro de forma segura entre hilos.
Este diseño asegura que las tareas independientes se ejecuten en paralelo, optimizando el rendimiento del sistema.
*/