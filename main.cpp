// --- IMPORTANTE: definir versión de Windows antes de incluir httplib ---
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

// Incluir primero httplib (que arrastra windows.h)
#include "httplib.h"
#include <iostream>
#include <string>
#include <chrono>
#include <random>
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


int main() {
    httplib::Server svr;
    cargar_sesiones_iniciales();
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
            // Guardamos en el LinearHash: key (parametro functino hasheo) = token, value = Sesion
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
        // valida token sea existente
        Sesion sesion;
        if (!tablaSesiones.try_get(token, sesion)) {
            json err;
            err["mensaje"] = "Token invalido o no encontrado";
            res.set_content(err.dump(), "application/json");
            res.status = 401;
            cout << "[SERVICIO][ERROR] token no encontrado en tabla\n";
            // Imprimir estado actual de la tabla (para ver que realmente no está)
            tablaSesiones.debug_print("SERVICIO - token no encontrado");
            return;
        }
        // valida token siga vigente
        auto ahora = std::chrono::system_clock::now();
        auto diff_min =
            std::chrono::duration_cast<std::chrono::minutes>(ahora - sesion.creada_en)
                .count();
        cout << "[SERVICIO] token encontrado. Minutos desde creación=" << diff_min << "\n";
        if (diff_min > 60) { // más de 1 hora
            // delete token si ya no está vigente
            cout << "[SERVICIO] token EXPIRADO, se eliminara de la tabla\n";
            tablaSesiones.remove(token);
            tablaSesiones.debug_print("DESPUES DE eliminar token EXPIRADO en /servicio");
            json resp;
            resp["mensaje"] = "Sesion terminada, vuelva a loguearse";
            res.set_content(resp.dump(), "application/json");
            res.status = 401;
            return;
        }

        // Token válido y no expirado
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
        // no usamos BODY
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
    svr.listen("0.0.0.0", 8080);
    return 0;
}
