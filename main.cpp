// --- IMPORTANTE: definir versión de Windows antes de incluir httplib ---
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

// Incluir primero httplib (que arrastra windows.h)
#include "httplib.h"
#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <fstream>
#include <thread>
#include <mutex>
#include "linearhash.h"
#include "json.hpp"

using json = nlohmann::json;
// Modelo de sesión que se guarda en el LinearHash
struct Sesion {
    std::string correo;
    std::string password;
    std::chrono::system_clock::time_point creada_en;
};

// Tabla global de sesiones (usa LinearHash.h)
LinearHash<std::string, Sesion> tablaSesiones(4);

std::mutex tablaSesionesMutex;

// Generar token único
std::string generar_token() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(std::random_device{}());
    uint64_t r = rng();
    return std::to_string(now) + "_" + std::to_string(r);
}

void cargar_sesiones_iniciales() {
    cout << "[BOOT] Cargando sesiones iniciales (INGESTA DE DATOS)...\n";
    std::vector<std::pair<std::string, std::string>> usuarios = {
        {"user01@test.com", "pass01"},
        {"user02@test.com", "pass02"},
        {"user03@test.com", "pass03"},
        {"user04@test.com", "pass04"},
        {"user05@test.com", "pass05"},
        {"user06@test.com", "pass06"},
        {"user07@test.com", "pass07"},
        {"user08@test.com", "pass08"},
        {"user09@test.com", "pass09"},
        {"user10@test.com", "pass10"},
        {"user11@test.com", "pass11"},
        {"user12@test.com", "pass12"},
        {"user13@test.com", "pass13"},
        {"user14@test.com", "pass14"},
        {"user15@test.com", "pass15"},
        {"user16@test.com", "pass16"},
        {"user17@test.com", "pass17"},
        {"user18@test.com", "pass18"},
        {"user19@test.com", "pass19"},
        {"user20@test.com", "pass20"}
    };
    for (const auto& u : usuarios) {
        const std::string& correo   = u.first;
        const std::string& password = u.second;
        std::string token = generar_token();
        Sesion sesion{
            correo,
            password,
            std::chrono::system_clock::now()
        };
        tablaSesiones.insert(token, sesion);
        cout << "[BOOT] Sesion inicial insertada -> correo=" << correo
             << "  token=" << token << "\n";
    }
    tablaSesiones.debug_print("DESPUES DE CARGA INICIAL (20 sesiones)");
}

void limpiar_sesiones_expiradas() {
    std::lock_guard<std::mutex> lock(tablaSesionesMutex);
    auto ahora = std::chrono::system_clock::now();
    
    cout << "[CLEANUP] Recorriendo tabla para buscar sesiones expiradas (>5 minutos)...\n";
    
    int eliminados = tablaSesiones.for_each_remove_if([&ahora](const std::string& token, Sesion& sesion) -> bool {
        auto diff_min = std::chrono::duration_cast<std::chrono::minutes>(ahora - sesion.creada_en).count();
        if (diff_min > 5) {
            cout << "[CLEANUP] Token expirado: " << token << " (expirado hace " << diff_min - 5 << " minutos)\n";
            return true;
        }
        return false;
    });
    
    if (eliminados > 0) {
        cout << "[CLEANUP] Se eliminaron " << eliminados << " sesiones expiradas\n";
        tablaSesiones.debug_print("DESPUES DE LIMPIEZA AUTOMATICA");
    } else {
        cout << "[CLEANUP] No se encontraron sesiones expiradas\n";
    }
}

void hilo_limpieza_periodica() {
    const int intervalo_limpieza_segundos = 300;
    
    cout << "\n[CLEANUP] Hilo de limpieza automática INICIADO\n";
    cout << "[CLEANUP] Se ejecutará cada 5 minutos\n\n";
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(intervalo_limpieza_segundos));
        cout << "[CLEANUP] Ejecutando limpieza automática...\n";
        limpiar_sesiones_expiradas();
    }
}

int main() {
    httplib::Server svr;
    cargar_sesiones_iniciales();

    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                             {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                             {"Access-Control-Allow-Headers", "Content-Type, Authorization"},
                             {"Access-Control-Max-Age", "3600"}});
    
    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
    });

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file("../index.html", std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            res.set_content(content, "text/html");
            res.status = 200;
        } else {
            res.status = 404;
        }
    });
    
    svr.Get("/styles.css", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file("../styles.css", std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            res.set_content(content, "text/css");
            res.status = 200;
        } else {
            res.status = 404;
        }
    });
    
    svr.Get("/app.js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file("../app.js", std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            res.set_content(content, "application/javascript");
            res.status = 200;
        } else {
            res.status = 404;
        }
    });

    // 1. LOGIN
    // POST /login
    // Body JSON: { "correo": "...", "password": "..." }
    // Respuesta: { "token": "..." }
    svr.Post("/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string correo   = body.at("correo").get<std::string>();
            std::string password = body.at("password").get<std::string>();
            std::string token = generar_token();
            Sesion sesion {
                correo,
                password,
                std::chrono::system_clock::now()
            };
            cout << "[LOGIN] correo=" << correo << " password=" << password << "\n";
            cout << "[LOGIN] token generado=" << token << "\n";
            tablaSesiones.insert(token, sesion);
            tablaSesiones.debug_print("DESPUES DE /login (insert)");
            json resp;
            resp["token"] = token;
            res.set_content(resp.dump(), "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            json err;
            err["mensaje"] = "Error en login";
            err["detalle"] = e.what();
            res.set_content(err.dump(), "application/json");
            res.status = 400;
            cout << "[LOGIN][ERROR] " << e.what() << "\n";
        }
    });

    // 2. SERVICIO PROTEGIDO
    // GET /servicio?token=XXXX
    // - Si token no existe -> 401
    // - Si existe pero token ya paso > 1 hora -> se borra y 401 "sesión terminada"
    // - Si tod0 OK -> 200 "acceso permitido"
    svr.Get("/servicio", [](const httplib::Request& req, httplib::Response& res) {
        std::string token;
        if (req.has_param("token")) {
            token = req.get_param_value("token");
        }
        cout << "[SERVICIO] llamado con token=" << token << "\n";
        if (token.empty()) {
            json err;
            err["mensaje"] = "Token requerido";
            res.set_content(err.dump(), "application/json");
            res.status = 401;
            cout << "[SERVICIO][ERROR] token vacio\n";
            return;
        }
        Sesion sesion;
        if (!tablaSesiones.try_get(token, sesion)) {
            json err;
            err["mensaje"] = "Token invalido o no encontrado";
            res.set_content(err.dump(), "application/json");
            res.status = 401;
            cout << "[SERVICIO][ERROR] token no encontrado en tabla\n";
            tablaSesiones.debug_print("SERVICIO - token no encontrado");
            return;
        }
        auto ahora = std::chrono::system_clock::now();
        auto diff_min =
            std::chrono::duration_cast<std::chrono::minutes>(ahora - sesion.creada_en)
                .count();
        cout << "[SERVICIO] token encontrado. Minutos desde creación=" << diff_min << "\n";
        if (diff_min > 5) {
            cout << "[SERVICIO] token EXPIRADO, se eliminara de la tabla\n";
            tablaSesiones.remove(token);
            tablaSesiones.debug_print("DESPUES DE eliminar token EXPIRADO en /servicio");
            json resp;
            resp["mensaje"] = "Sesion terminada, vuelva a loguearse";
            res.set_content(resp.dump(), "application/json");
            res.status = 401;
            return;
        }
        json ok;
        ok["mensaje"] = "Acceso permitido";
        ok["correo"]  = sesion.correo;
        res.set_content(ok.dump(), "application/json");
        res.status = 200;
        cout << "[SERVICIO] acceso permitido para correo=" << sesion.correo << "\n";
    });

    // 3. LOGOUT
    // POST /logout
    // Body JSON: { "token": "..." }
    // Borra SOLO esa sesión
    svr.Post("/logout", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string token = body.at("token").get<std::string>();
            cout << "[LOGOUT] token=" << token << "\n";
            bool eliminado = tablaSesiones.remove(token);
            tablaSesiones.debug_print("DESPUES DE /logout (remove)");
            json resp;
            if (eliminado) {
                resp["mensaje"] = "Sesion cerrada correctamente";
                res.status = 200;
                cout << "[LOGOUT] sesion eliminada\n";
            } else {
                resp["mensaje"] = "Token no encontrado";
                res.status = 404;
                cout << "[LOGOUT][WARN] token no existia en la tabla\n";
            }
            res.set_content(resp.dump(), "application/json");
        }
        catch (...) {
            json err;
            err["mensaje"] = "Error en logout";
            res.set_content(err.dump(), "application/json");
            res.status = 400;
            cout << "[LOGOUT][ERROR] excepcion al parsear body\n";
        }
    });

    // 4. CLEAR GLOBAL (ADMIN)
    // POST /admin/clear
    // Sin body. Borra TODAS las sesiones.
    svr.Post("/admin/clear", [](const httplib::Request& req, httplib::Response& res) {
        (void)req; cout << "[ADMIN/CLEAR] se eliminaran TODAS las sesiones\n";
        tablaSesiones.clear();
        tablaSesiones.debug_print("DESPUES DE /admin/clear (clear)");
        json resp;
        resp["mensaje"] = "Todas las sesiones han sido eliminadas";
        res.set_content(resp.dump(), "application/json");
        res.status = 200;
    });

    std::cout << "Servidor escuchando en http://localhost:8080\n";
    tablaSesiones.debug_print("ESTADO INICIAL (tabla ingestada)");
    
    std::thread cleanup_thread(hilo_limpieza_periodica);
    cleanup_thread.detach();
    
    svr.listen("0.0.0.0", 8080);
    return 0;
}
