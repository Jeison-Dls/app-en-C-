#include <iostream>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include "globals.h"
#include "turnos.h"
#include "globals.h"
#include "colorear.h"// Archivo que contiene la funcion `colorear`
#include <fstream>
#include <filesystem> // Para verificar si el archivo existe
#include <condition_variable>


void inicializarArchivoTurnos();

// Variables globales para sincronización
std::mutex mtxArchivo;
std::condition_variable cvArchivo;
bool turnoListo = false;

// Verificar si el archivo ya existe
bool archivoExiste(const std::string& nombreArchivo) {
    return std::filesystem::exists(nombreArchivo);
}

// Crear el archivo de turnos si no existe
void inicializarArchivoTurnos() {
    const std::string nombreArchivo = "turnos.txt";
    if (!std::filesystem::exists(nombreArchivo)) {
        std::ofstream archivo(nombreArchivo);
        if (archivo.is_open()) {
            archivo << "=== Lista de Turnos ===\n";
            archivo.close();
            std::cout << colorear("Archivo de turnos creado correctamente.\n", VERDE);
        } else {
            std::cerr << colorear("Error al crear el archivo de turnos.\n", ROJO);
        }
    }
}
// Funcion para consultar la tabla de turnos
void consultarTurnos(sqlite3* db) {
    std::unique_lock<std::mutex> lock(mtx);

    const char* sql = R"(
        SELECT t.id AS turno_id, t.fecha, t.hora, 
               m.nombre AS medico_nombre, m.especialidad AS medico_especialidad,
               p.nombre AS paciente_nombre, p.edad AS paciente_edad,
               t.estado
        FROM turnos t
        JOIN medicos m ON t.medico_id = m.id
        JOIN pacientes p ON t.paciente_id = p.id;
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para turnos: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << colorear("\n=== Lista de Turnos ===\n", AZUL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int turnoId = sqlite3_column_int(stmt, 0);
        std::string fecha = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string hora = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string medicoNombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        std::string medicoEspecialidad = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        std::string pacienteNombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        int pacienteEdad = sqlite3_column_int(stmt, 6);
        std::string estado = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

        std::cout << colorear("ID del Turno: ", VERDE) << turnoId << "\n"
                  << colorear("Fecha: ", VERDE) << fecha << "\n"
                  << colorear("Hora: ", VERDE) << hora << "\n"
                  << colorear("Medico: ", VERDE) << medicoNombre << " (" << medicoEspecialidad << ")\n"
                  << colorear("Paciente: ", VERDE) << pacienteNombre << " (Edad: " << pacienteEdad << ")\n"
                  << colorear("Estado: ", VERDE) << estado << "\n\n";
    }

    sqlite3_finalize(stmt);
}


// Funcion para mostrar medicos y sus horarios disponibles
void mostrarMedicosConHorarios(sqlite3* db) {
    std::unique_lock<std::mutex> lock(mtx);
    const char* sql = "SELECT id, nombre, especialidad, horarios_disponibles FROM medicos;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para medicos: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << colorear("\n=== Medicos Disponibles ===\n", AZUL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string nombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string especialidad = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string horarios = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        std::cout << colorear("ID: ", VERDE) << id << "\n"
                  << colorear("Nombre: ", VERDE) << nombre << "\n"
                  << colorear("Especialidad: ", VERDE) << especialidad << "\n"
                  << colorear("Horario Disponible: ", VERDE) << horarios << "\n\n";
    }

    sqlite3_finalize(stmt);
}

// Funcion para mostrar pacientes disponibles
void mostrarPacientesDisponibles(sqlite3* db) {
    std::unique_lock<std::mutex> lock(mtx);
    const char* sqlPacientes = "SELECT id, nombre FROM pacientes WHERE medico_asignado IS NULL OR medico_asignado = 'Sin asignar';";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sqlPacientes, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para pacientes: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << colorear("\n=== Pacientes Disponibles ===\n", AZUL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string nombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::cout << colorear("ID: ", VERDE) << id << colorear(" | Nombre: ", VERDE) << nombre << "\n";
    }

    sqlite3_finalize(stmt);
}

// Agregar un nuevo turno al archivo
void agregarTurnoAlArchivo(sqlite3* db, int turnoId) {
    std::unique_lock<std::mutex> lock(mtxArchivo);

    // Esperar hasta que el turno esté registrado
    cvArchivo.wait(lock, [] { return turnoListo; });

    sqlite3* localDb;
    if (sqlite3_open_v2("./database/hospital.db", &localDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        std::cerr << colorear("Error al abrir la base de datos para agregar turno al archivo: ", ROJO) << sqlite3_errmsg(localDb) << std::endl;
        return;
    }

    const char* sql = R"(
        SELECT t.id AS turno_id, t.fecha, t.hora, 
               m.nombre AS medico_nombre, m.especialidad AS medico_especialidad,
               p.nombre AS paciente_nombre, p.edad AS paciente_edad,
               t.estado
        FROM turnos t
        JOIN medicos m ON t.medico_id = m.id
        JOIN pacientes p ON t.paciente_id = p.id
        WHERE t.id = ?;
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(localDb, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para agregar turno al archivo: ", ROJO) << sqlite3_errmsg(localDb) << std::endl;
        sqlite3_close(localDb);
        return;
    }

    sqlite3_bind_int(stmt, 1, turnoId);

    std::ofstream archivo("turnos.txt", std::ios::app); // Abrir en modo añadir
    if (!archivo.is_open()) {
        std::cerr << colorear("Error al abrir el archivo de turnos para escribir.\n", ROJO);
        sqlite3_finalize(stmt);
        sqlite3_close(localDb);
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string fecha = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string hora = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string medicoNombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        std::string medicoEspecialidad = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        std::string pacienteNombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        int pacienteEdad = sqlite3_column_int(stmt, 6);
        std::string estado = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

        // Agregar los datos del turno al archivo
        archivo << "ID del Turno: " << id << "\n"
                << "Fecha: " << fecha << "\n"
                << "Hora: " << hora << "\n"
                << "Medico: " << medicoNombre << " (" << medicoEspecialidad << ")\n"
                << "Paciente: " << pacienteNombre << " (Edad: " << pacienteEdad << ")\n"
                << "Estado: " << estado << "\n\n";
    } else {
        std::cerr << colorear("No se encontró el turno para agregar al archivo.\n", ROJO);
    }

    archivo.close();
    sqlite3_finalize(stmt);
    sqlite3_close(localDb);
}


// Registrar el turno
void registrarTurno(sqlite3* db, int medicoId, int pacienteId, const std::string& fecha, const std::string& hora) {
    std::unique_lock<std::mutex> lock(mtx);

    const char* sqlInsertTurno = R"(
        INSERT INTO turnos (fecha, hora, medico_id, paciente_id)
        VALUES (?, ?, ?, ?);
    )";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sqlInsertTurno, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para insertar turno: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, fecha.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hora.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, medicoId);
    sqlite3_bind_int(stmt, 4, pacienteId);

    int turnoId = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        turnoId = sqlite3_last_insert_rowid(db); // Obtener ID del turno recién creado
        std::cout << colorear("Turno registrado correctamente.\n", VERDE);

        // Notificar que el turno está listo para agregarse al archivo
        {
            std::lock_guard<std::mutex> lockArchivo(mtxArchivo);
            turnoListo = true;
        }
        cvArchivo.notify_one();

        // Lanzar hilo para agregar el turno al archivo
        std::thread agregarArchivoThread([=]() { agregarTurnoAlArchivo(db, turnoId); });
        agregarArchivoThread.detach();
    } else {
        std::cerr << colorear("Error al registrar el turno: ", ROJO) << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
}
// Funcion para actualizar el campo medico_asignado
void actualizarMedicoAsignado(sqlite3* db, int medicoId, int pacienteId) {
    std::unique_lock<std::mutex> lock(mtx);
    const char* sqlUpdatePaciente = "UPDATE pacientes SET medico_asignado = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sqlUpdatePaciente, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta para actualizar paciente: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, medicoId);
    sqlite3_bind_int(stmt, 2, pacienteId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << colorear("Error al actualizar el paciente: ", ROJO) << sqlite3_errmsg(db) << std::endl;
    } else {
        std::cout << colorear("El paciente ha sido asignado al medico correctamente.\n", VERDE);
    }

    sqlite3_finalize(stmt);
}

// Funcion principal para registrar turnos
void registrarTurnoConHilos(sqlite3* db) {
    int medicoId, pacienteId;
    std::string fecha, hora;

    // Lanzar hilos para consultas independientes
    std::thread consultaTurnos([&]() { consultarTurnos(db); }); // Hilo para consultar turnos
    std::thread medicosThread([&]() { mostrarMedicosConHorarios(db); }); // Hilo para mostrar médicos

    // Esperar que ambos hilos terminen (se ejecutan en paralelo)
    consultaTurnos.join(); // Unir el hilo de consulta de turnos
    medicosThread.join(); // Unir el hilo de consulta de médicos

    // Solicitar ID del medico
    std::cout << colorear("Ingrese el ID del medico: ", AMARILLO);
    std::cin >> medicoId;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Lanzar hilo para mostrar pacientes mientras se ingresa el ID del medico
    std::thread pacientesThread([&]() { mostrarPacientesDisponibles(db); });

    // Esperar a que termine el hilo de pacientes
    pacientesThread.join();

    // Solicitar ID del paciente, fecha y hora
    std::cout << colorear("\nIngrese el ID del paciente: ", AMARILLO);
    std::cin >> pacienteId;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << colorear("Ingrese la fecha del turno (YYYY-MM-DD): ", AMARILLO);
    std::getline(std::cin, fecha);

    std::cout << colorear("Ingrese la hora del turno (HH:MM): ", AMARILLO);
    std::getline(std::cin, hora);

    // Lanzar hilos para registrar turno y actualizar medico asignado en paralelo
    std::thread registrarThread([&]() { registrarTurno(db, medicoId, pacienteId, fecha, hora); });
    std::thread actualizarThread([&]() { actualizarMedicoAsignado(db, medicoId, pacienteId); });

    // Esperar a que ambos hilos terminen
    registrarThread.join();
    actualizarThread.join();
}


// Menú para la gestion de turnos
void menuTurnos(sqlite3* db) {
    int opcion;
    do {
        std::cout << colorear("\n=== Gestion de Turnos ===\n\n", AZUL);
        std::cout << colorear("1. Registrar Turno\n", VERDE);
        std::cout << colorear("2. Volver al Menú Principal\n\n", VERDE);
        std::cout << colorear("Seleccione una opcion: ", BLANCO);
        std::cin >> opcion;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (opcion) {
            case 1:
                registrarTurnoConHilos(db);
                break;
            case 2:
                return;
            default:
                std::cout << colorear("Opcion no valida.\n", ROJO);
        }
    } while (opcion != 2);
}
/*
Resumen del paralelismo

Consultas iniciales en paralelo:
consultaTurnos y medicosThread ejecutan consultas a la base de datos de forma paralela.
El hilo pacientesThread se ejecuta mientras el usuario proporciona el ID del médico.

Registro y actualización en paralelo:
El registro de turno (registrarThread) y la actualización del médico asignado (actualizarThread) se ejecutan al mismo tiempo.

Escritura en archivo en paralelo:
Un hilo separado (agregarArchivoThread) escribe el turno recién registrado en un archivo de texto, mientras el programa continúa con otras operaciones.

Sincronización:
std::mutex, std::condition_variable, y std::lock_guard protegen las operaciones críticas y coordinar los hilos.
Utilizas detach para que el hilo de escritura no bloquee la ejecución principal.

 */