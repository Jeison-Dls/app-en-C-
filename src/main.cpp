#include <iostream>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iomanip> // Para formatear la salida
#include "medicos.h"
#include "pacientes.h"
#include "turnos.h"
#include "colorear.h"// Archivo que contiene la función `colorear`

// Variables globales para sincronizacion
std::mutex mtx;
std::condition_variable cv;
bool validacionExitosa = false;

// Prototipos
void mostrarTitulo(const std::string &titulo);
void registrarUsuario(sqlite3 *db);
void validarUsuario();
void iniciarSesion(sqlite3 *db);



// Funcion principal
int main()
{
    // Configurar SQLite para modo multihilo
    if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD) != SQLITE_OK) {
        std::cerr << "Error al configurar SQLite para acceso multihilo.\n";
        return -1;
    }


    sqlite3 *db;
    int exit = sqlite3_open_v2("./database/hospital.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (exit) {
        std::cerr << colorear("Error al abrir la base de datos: ", ROJO) << sqlite3_errmsg(db) << std::endl;
        return exit;
    }

    // Crear la tabla si no existe
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS usuarios (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL,
            username TEXT NOT NULL UNIQUE,
            nombreCompleto TEXT NOT NULL,
            password TEXT NOT NULL
        );
    )";

    char *mensajeError;
    if (sqlite3_exec(db, sql, 0, 0, &mensajeError) != SQLITE_OK)
    {
        std::cerr << colorear("Error al crear la tabla usuarios: ", ROJO) << mensajeError << std::endl;
        sqlite3_free(mensajeError);
        return -1;
    }

    // Crear la tabla de medicos si no existe
    const char* sqlMedicos = R"(
    CREATE TABLE IF NOT EXISTS medicos (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        nombre TEXT NOT NULL,
        especialidad TEXT NOT NULL,
        telefono TEXT NOT NULL,
        email TEXT NOT NULL,
        experiencia INTEGER NOT NULL,
        rol TEXT,
        horarios_disponibles TEXT
    );
)";
    if (sqlite3_exec(db, sqlMedicos, 0, 0, &mensajeError) != SQLITE_OK)
    {
        std::cerr << colorear("Error al crear la tabla de medicos: ", ROJO) << mensajeError << std::endl;
        sqlite3_free(mensajeError);
        return -1;
    }

    const char *sqlPacientes = R"(
        CREATE TABLE IF NOT EXISTS pacientes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            nombre TEXT NOT NULL,
            edad INTEGER NOT NULL,
            genero TEXT NOT NULL CHECK(genero IN ('F', 'M')),
            telefono TEXT NOT NULL,
            email TEXT NOT NULL,
            prioridad TEXT,
            medico_asignado TEXT DEFAULT 'Sin asignar'
        );
    )";
    
    if (sqlite3_exec(db, sqlPacientes, 0, 0, &mensajeError) != SQLITE_OK)
    {
        std::cerr << colorear("Error al crear la tabla de pacientes: ", ROJO) << mensajeError << std::endl;
        sqlite3_free(mensajeError);
        return -1;
    }

    const char* sqlTurnos = R"(
    CREATE TABLE IF NOT EXISTS turnos (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        fecha TEXT NOT NULL,
        hora TEXT NOT NULL,
        medico_id INTEGER NOT NULL,
        paciente_id INTEGER NOT NULL,
        estado TEXT NOT NULL DEFAULT 'pendiente',
        FOREIGN KEY (medico_id) REFERENCES medicos(id),
        FOREIGN KEY (paciente_id) REFERENCES pacientes(id)
    );
)";
if (sqlite3_exec(db, sqlTurnos, 0, 0, &mensajeError) != SQLITE_OK) {
    std::cerr << colorear("Error al crear la tabla de turnos: ", ROJO) << mensajeError << std::endl;
    sqlite3_free(mensajeError);
    return -1;
}

    int opcion;
    do
    {
        mostrarTitulo(colorear("\nSistema de Gestion de Turnos Hospitalarios",AZUL));

        std::cout << colorear("1. Registrar Usuario\n", VERDE);
        std::cout << colorear("2. Iniciar Sesion\n", VERDE);
        std::cout << colorear("3. Salir\n", AMARILLO);
        std::cout << colorear("\nSeleccione una opcion: ", BLANCO);
        std::cin >> opcion;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Limpiar el buffer

        switch (opcion)
        {
        case 1:
        {
            // Crear un hilo para ejecutar la función `registrarUsuario`
            std::thread hiloRegistro([&db]()
                                     { registrarUsuario(db); });

            // Espera a que el registro termine antes de proceder a validar
            hiloRegistro.join();

            // Crear un segundo hilo para ejecutar la función `validarUsuario`
            std::thread hiloValidacion([&]()
                                       { validarUsuario(); });

            // Espera a que el hilo de validación termine
            hiloValidacion.join();

            // Si la validacion es exitosa, abrir el login
            if (validacionExitosa)
            {
                iniciarSesion(db);
            }
            else
            {
                std::cout << colorear("No se puede proceder al login. Validacion fallida.\n", ROJO);
            }

            break;
        }
        case 2:
            iniciarSesion(db);
            break;
        case 3:
            std::cout << colorear("Gracias por usar el sistema. Hasta luego\n", MAGENTA);
            break;
        default:
            std::cout << colorear("Opcion no valida. Intente de nuevo.\n", ROJO);
        }
    } while (opcion != 3);

    sqlite3_close(db);
    return 0;
}

// Funcion para mostrar un título con bordes
void mostrarTitulo(const std::string &titulo)
{
    std::cout << colorear("\n========================================\n", CIAN);
    std::cout << std::setw(20) << std::right << titulo << "\n";
    std::cout << colorear("\n========================================\n\n", CIAN);
}

// Funcion del menú principal
void menuPrincipal(const std::string &usuario, sqlite3 *db)
{
    int opcion;
    do
    {
        mostrarTitulo(colorear("Sistema de Gestion de Turnos Hospitalarios", AZUL));
        std::cout << colorear("Usuario: ",AMARILLO) << usuario << "\n";
        std::cout << colorear("\n========================================\n", CIAN);
        std::cout << colorear("1. Registro de Medicos\n", VERDE);
        std::cout << colorear("2. Registro de Pacientes\n",VERDE);
        std::cout << colorear("3. Gestion de turnos\n", VERDE);
        std::cout << colorear("4. Cerrar Sesion\n", AMARILLO);
        std::cout << colorear("\nSeleccione una opcion: ", BLANCO);
        std::cin >> opcion;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Limpiar el buffer

        switch (opcion)
        {
        case 1:
            menuMedicos(db); // Llamar al menú de medicos
            break;
        case 2:
            menuPacientes(db); // Llamar al menú de pacientes
            break;

        case 3:
            menuTurnos(db);// Llamar al menú de turnos
            break;
        case 4:
            std::cout << colorear( "Cerrando sesion...\n", ROJO);
            return; // Salir del menú
        default:
            std::cout << colorear("Opcion no valida. Intente de nuevo.\n", ROJO);
        }
    } while (opcion != 3);
}

// Funcion para registrar un usuario
void registrarUsuario(sqlite3 *db)
{
    // Bloqueo único para sincronizar con `std::condition_variable`
    std::unique_lock<std::mutex> lock(mtx);

    // Código para recolectar datos y registrar al usuario
    std::string email, username, nombreCompleto, password, confirmarPassword;

    std::cout << colorear("Ingrese email: ", VERDE);
    std::getline(std::cin, email);

    std::cout << colorear("Ingrese nombre de usuario: ", VERDE);
    std::getline(std::cin, username);

    std::cout << colorear("Ingrese nombre completo: ", VERDE);
    std::getline(std::cin, nombreCompleto);

    std::cout << colorear("Ingrese contrasena: ", VERDE);
    std::getline(std::cin, password);

    std::cout << colorear("Confirme su contrasena: ", VERDE);
    std::getline(std::cin, confirmarPassword);

    // Validar que los campos no esten vacíos
    if (email.empty() || username.empty() || nombreCompleto.empty() ||
        password.empty() || confirmarPassword.empty())
    {
        std::cerr << colorear("Error: Todos los campos son obligatorios.\n", ROJO);
        validacionExitosa = false;
        cv.notify_one(); // Notificar a los hilos en espera
        return;
    }

    // Validar que las contrasenas coincidan
    if (password != confirmarPassword)
    {
        std::cerr << colorear("Error: Las contrasenas no coinciden.\n", ROJO);
        validacionExitosa = false;
        cv.notify_one(); // Notificar a los hilos en espera de validación
        return;
    }

    // Verificar si el username ya existe
    const char *checkUserSql = "SELECT COUNT(*) FROM usuarios WHERE username = ?;";
    sqlite3_stmt *checkStmt;

    if (sqlite3_prepare_v2(db, checkUserSql, -1, &checkStmt, 0) != SQLITE_OK)
    {
        std::cerr << colorear("Error al preparar la consulta para verificar el usuario: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        validacionExitosa = false;
        cv.notify_one();
        return;
    }

    sqlite3_bind_text(checkStmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(checkStmt) == SQLITE_ROW)
    {
        int count = sqlite3_column_int(checkStmt, 0);
        if (count > 0)
        {
            std::cerr << colorear("Error: El nombre de usuario ya esta en uso. Por favor, elija otro.\n", ROJO);
            validacionExitosa = false;
            sqlite3_finalize(checkStmt);
            cv.notify_one();
            return;
        }
    }
    sqlite3_finalize(checkStmt);

    // Preparar la consulta SQL para registrar al usuario
    const char *sql = "INSERT INTO usuarios (email, username, nombreCompleto, password) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        std::cerr << colorear("Error al preparar la consulta: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        validacionExitosa = false;
        cv.notify_one();
        return;
    }

    // Enlazar valores a la consulta
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, nombreCompleto.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, password.c_str(), -1, SQLITE_STATIC);

    // Ejecutar la consulta
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::cerr << colorear("Error al registrar el usuario: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        validacionExitosa = false;
    }
    else
    {
        std::cout << colorear("Usuario registrado correctamente.\n", VERDE);
        validacionExitosa = true;
    }

    cv.notify_one(); // Notificar al hilo de validacion que puede continuar
    sqlite3_finalize(stmt);
}

// Funcion para validar los datos del usuario
void validarUsuario()
{
    // Bloqueo único para sincronización con `std::mutex`
    std::unique_lock<std::mutex> lock(mtx);

    // Verificar si la validación fue exitosa
    if (validacionExitosa)
    {
        std::cout << colorear("Validacion exitosa. Usuario registrado correctamente.\n", VERDE);
    }
    else
    {
        std::cout << colorear("Validacion fallida. No se pudo registrar al usuario.\n", ROJO);
    }
}

// Funcion para iniciar sesion
void iniciarSesion(sqlite3 *db)
{
    mostrarTitulo(colorear("Inicio de Sesion", AZUL));

    std::string username, password;

    std::cout << colorear("Ingrese Usuario: ", VERDE);
    std::getline(std::cin, username);

    std::cout << colorear("Ingrese contrasena: ", VERDE);
    std::getline(std::cin, password);

    const char *sql = "SELECT * FROM usuarios WHERE username = ? AND password = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        std::cerr << colorear("Error al preparar la consulta: ", ROJO)  << sqlite3_errmsg(db) << std::endl;
        return;
    }

    // Enlazar los valores de usuario y contrasena
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    // Verificar si se encontro un usuario con los datos proporcionados
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::cout << colorear("Inicio de sesion exitoso. --Bienvenido(a), ", VERDE)  << username << "!\n";
        menuPrincipal(username, db); // Llamar al menú principal despues de iniciar sesion
    }
    else
    {
        std::cout << colorear("Usuario o contrasena incorrectos.\n", ROJO);
    }

    sqlite3_finalize(stmt);
}

/*Resumen del paralelismo en mi código


Estoy ejecutando dos tareas en paralelo:
    Registro de usuario (registrarUsuario) en un hilo.
    Validación de usuario (validarUsuario) en otro hilo.

utilizo std::mutex y std::condition_variable para sincronizar y comunicar entre los hilos.
el hilo principal crea un hilo para registrar un usuario y luego crea otro hilo para validar el usuario.
Cada hilo realiza una tarea independiente, pero están coordinados para evitar condiciones de carrera. */