#include <iostream>
#include <string>
#include <sqlite3.h>
#include <thread>
#include <vector>
#include <mutex>
#include "globals.h"
#include "colorear.h"// Archivo que contiene la funcion `colorear`

// Funcion para consultar la tabla de medicos antes del registro
void consultarMedicosAntesRegistro(sqlite3* db) {
    std::unique_lock<std::mutex> lock(mtx); // Sincronización con otros hilos

    const char* sqlCheck = "SELECT COUNT(*) FROM medicos;";
    sqlite3_stmt* stmt;

    // Preparar y ejecutar la consulta para contar médicos
    if (sqlite3_prepare_v2(db, sqlCheck, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al verificar la tabla de medicos: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
        // Mostrar la tabla de medicos
        const char* sql = "SELECT id, nombre, especialidad, telefono, email, experiencia, rol, horarios_disponibles FROM medicos;";
        sqlite3_stmt* stmtConsulta;

        if (sqlite3_prepare_v2(db, sql, -1, &stmtConsulta, 0) != SQLITE_OK) {
            std::cerr << colorear("Error al preparar la consulta para medicos: ", ROJO) << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return;
        }

        std::cout << colorear("\n=== Lista de Medicos Registrados ===\n", AZUL);
        while (sqlite3_step(stmtConsulta) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmtConsulta, 0);
            std::string nombre = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 1));
            std::string especialidad = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 2));
            std::string telefono = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 3));
            std::string email = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 4));
            bool experiencia = sqlite3_column_int(stmtConsulta, 5);
            std::string rol = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 6));
            std::string horarios_disponibles = reinterpret_cast<const char*>(sqlite3_column_text(stmtConsulta, 7));

            std::cout << colorear("ID: ", VERDE) << id << "\n"
                      << colorear("Nombre: ", VERDE) << nombre << "\n"
                      << colorear("Especialidad: ", VERDE) << especialidad << "\n"
                      << colorear("Telefono: ", VERDE) << telefono << "\n"
                      << colorear("Email: ", VERDE) << email << "\n"
                      << colorear("Experiencia: ", VERDE) << (experiencia ? "Si" : "No") << "\n"
                      << colorear("Rol: ", VERDE) << rol << "\n"
                      << colorear("Horario Disponible: ", VERDE) << horarios_disponibles << "\n\n";
        }
        sqlite3_finalize(stmtConsulta);
    }
    sqlite3_finalize(stmt);
}

// Funcion para registrar un medico
void registrarMedico(sqlite3* db, bool& registroExitoso, int& medicoId) {
    std::unique_lock<std::mutex> lock(mtx); // Sincronización con otros hilos
    std::string nombre, especialidad, telefono, email, horarioSeleccionado;
    bool tieneExperiencia;
    registroExitoso = false;

    // Solicitar datos al usuario y validarlos
    std::cout << colorear("Ingrese el nombre completo del medico: ", AMARILLO);
    std::getline(std::cin, nombre);

    std::cout << colorear("Ingrese la especialidad: ", AMARILLO);
    std::getline(std::cin, especialidad);

    std::cout << colorear("Ingrese el telefono: ", AMARILLO);
    std::getline(std::cin, telefono);

    std::cout << colorear("Ingrese el email: ", AMARILLO);
    std::getline(std::cin, email);

    std::cout << colorear("¿El medico tiene experiencia? (1 para Si, 0 para No): ", AMARILLO);
    std::cin >> tieneExperiencia;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Mostrar horarios disponibles
    std::cout << colorear("Seleccione un horario disponible:\n", AZUL);
    std::vector<std::string> horarios = {
        "08:00-12:00",
        "12:00-16:00",
        "16:00-20:00",
        "20:00-00:00"
    };
    for (size_t i = 0; i < horarios.size(); ++i) {
        std::cout << i + 1 << ". " << horarios[i] << "\n";
    }

    int opcionHorario;
    std::cout << colorear("Ingrese la opcion (1-", AMARILLO) << horarios.size() << "): ";
    std::cin >> opcionHorario;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (opcionHorario < 1 || opcionHorario > horarios.size()) {
        std::cerr << colorear("Error: Opcion no valida.\n", ROJO);
        return;
    }
    horarioSeleccionado = horarios[opcionHorario - 1];

    // Validar campos
    if (nombre.empty() || especialidad.empty() || telefono.empty() || email.empty()) {
        std::cerr << colorear("Error: Todos los campos son obligatorios.\n", ROJO);
        return;
    }

    // Insertar los datos en la base de datos
    const char* sql = R"(
        INSERT INTO medicos (nombre, especialidad, telefono, email, experiencia, rol, horarios_disponibles)
        VALUES (?, ?, ?, ?, ?, NULL, ?);
    )";
    sqlite3_stmt* stmt;

    // Preparar y ejecutar la consulta
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << colorear("Error al preparar la consulta: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return;
    }

    // Enlazar los valores
    sqlite3_bind_text(stmt, 1, nombre.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, especialidad.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, telefono.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, tieneExperiencia);
    sqlite3_bind_text(stmt, 6, horarioSeleccionado.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << colorear("Error al registrar al medico: ", ROJO) << sqlite3_errmsg(db) << std::endl;
    } else {
        registroExitoso = true;
        medicoId = sqlite3_last_insert_rowid(db); // Obtener el ID del medico registrado
        std::cout << colorear("Medico registrado correctamente con horario: ", VERDE) << horarioSeleccionado << "\n";
    }

    sqlite3_finalize(stmt);
}

// Funcion para asignar el rol automaticamente
void asignarRol(sqlite3* db, int medicoId) {
    std::unique_lock<std::mutex> lock(mtx); // Sincronización con otros hilos
    std::string rol;

    const char* sql = "SELECT experiencia FROM medicos WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "Error al preparar la consulta para asignar rol: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, medicoId);
    
    // Asignar rol basado en la experiencia

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        bool experiencia = sqlite3_column_int(stmt, 0);
        if (experiencia) {
            std::vector<std::string> rolesExperiencia = {"Cardiologo", "Neurocirujano", "Traumatologo"};
            rol = rolesExperiencia[medicoId % rolesExperiencia.size()];  // Asignar rol basado en ID
        } else {
            std::vector<std::string> rolesNoExperiencia = {"Medico General", "Asistente Medico", "Residente"};
            rol = rolesNoExperiencia[medicoId % rolesNoExperiencia.size()];  // Asignar rol basado en ID
        }
    } else {
        std::cerr << "No se encontro el medico con ID " << medicoId << ".\n";
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_finalize(stmt);

    const char* updateSql = "UPDATE medicos SET rol = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, 0) != SQLITE_OK) {
        std::cerr << "Error al preparar la consulta para actualizar rol: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, rol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, medicoId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error al asignar el rol: " << sqlite3_errmsg(db) << std::endl;
    } else {
        std::cout << "Rol asignado automaticamente: " << rol << "\n";
    }

    sqlite3_finalize(stmt);
}

// Menú para medicos
void menuMedicos(sqlite3* db) {
    bool registroExitoso = false;
    int medicoId = -1;

    // Crear un hilo para consultar los médicos existentes antes de registrar uno nuevo
    std::thread consulta([&]() { consultarMedicosAntesRegistro(db); });

    // Crear un hilo para registrar un nuevo médico
    std::thread registro([&]() { registrarMedico(db, registroExitoso, medicoId); });

    // Esperar que ambos hilos terminen antes de continuar
    consulta.join();
    registro.join();

    // Si el registro fue exitoso, crear un hilo para asignar automáticamente el rol
    if (registroExitoso) {
        std::thread asignacionRol([&]() { asignarRol(db, medicoId); });
        asignacionRol.join();
    }
}
/*Resumen del paralelismo en mi código
Hilos paralelos:

consultarMedicosAntesRegistro y registrarMedico se ejecutan en paralelo, ya que no dependen entre sí.
Mejora el rendimiento al evitar que una operación bloquee la otra.

Hilo condicional:

Si el registro es exitoso, un tercer hilo ejecuta asignarRol para asignar el rol al médico registrado.
Esto se hace condicionalmente para evitar asignar un rol si el registro falla.

Sincronización:
Se utilizan std::mutex y std::unique_lock para proteger las operaciones en la base de datos y evitar condiciones de carrera.
Se sincroniza el acceso a la base de datos para garantizar la coherencia de los datos.
Se utilizan std::thread para ejecutar tareas en paralelo y std::mutex para sincronizar el acceso a los datos compartidos.
 */