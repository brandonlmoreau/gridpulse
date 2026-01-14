#include "gridpulse/api_server.hpp"
#include "gridpulse/auth.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace gridpulse {

ApiServer::ApiServer(Database& db, int port)
    : db_(db), alert_checker_(db), port_(port) {
    setupRoutes();
}

// Helper to check string ending (C++17 compatible)
bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Helper to get MIME type
std::string getMimeType(const std::string& path) {
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".json")) return "application/json";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".svg")) return "image/svg+xml";
    return "text/plain";
}

// Helper to read file
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper to decode URL-encoded strings (handles %20 for spaces, etc.)
std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            result += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

void ApiServer::setupRoutes() {
    // Enable CORS for frontend
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });
    
    // Handle preflight OPTIONS requests
    server_.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
    
    // Serve static files - Dashboard
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        auto content = readFile("public/index.html");
        if (content.empty()) content = readFile("../public/index.html");
        if (!content.empty()) {
            res.set_content(content, "text/html");
        } else {
            res.status = 404;
            res.set_content("Dashboard not found. API is running at /api/*", "text/plain");
        }
    });
    
    server_.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        auto content = readFile("public/style.css");
        if (content.empty()) content = readFile("../public/style.css");
        if (!content.empty()) {
            res.set_content(content, "text/css");
        } else {
            res.status = 404;
        }
    });
    
    server_.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        auto content = readFile("public/app.js");
        if (content.empty()) content = readFile("../public/app.js");
        if (!content.empty()) {
            res.set_content(content, "application/javascript");
        } else {
            res.status = 404;
        }
    });
    
    // Health check
    server_.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
    });
    
    // Auth endpoints (public)
    server_.Post("/api/auth/register", [this](const httplib::Request& req, httplib::Response& res) {
        handleRegister(req, res);
    });
    
    server_.Post("/api/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleLogin(req, res);
    });
    
    server_.Get("/api/auth/me", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetMe(req, res);
    });
    
    // Device endpoints
    server_.Get("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetDevices(req, res);
    });
    
    server_.Get("/api/device", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetDevice(req, res);
    });
    
    server_.Post("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        handleCreateDevice(req, res);
    });
    
    server_.Post("/api/delete-device", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteDevice(req, res);
    });
    
    // Telemetry endpoints
    server_.Get("/api/telemetry", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTelemetry(req, res);
    });
    
    server_.Post("/api/telemetry", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostTelemetry(req, res);
    });
    
    // Alert endpoints
    server_.Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetAlerts(req, res);
    });
    
    server_.Post(R"(/api/alerts/(\d+)/ack)", [this](const httplib::Request& req, httplib::Response& res) {
        handleAckAlert(req, res);
    });
}

void ApiServer::run() {
    spdlog::info("Starting GridPulse API server on port {}", port_);
    server_.listen("0.0.0.0", port_);
}

void ApiServer::stop() {
    spdlog::info("Stopping API server");
    server_.stop();
}

void ApiServer::jsonResponse(httplib::Response& res, int status, const nlohmann::json& data) {
    res.status = status;
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.body = data.dump();
}

// Auth middleware - verifies JWT and returns user_id if valid
std::optional<int> ApiServer::authenticate(const httplib::Request& req, httplib::Response& res) {
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        errorResponse(res, 401, "Authorization header required");
        return std::nullopt;
    }
    
    auto token = Auth::extractBearerToken(auth_header);
    if (!token) {
        errorResponse(res, 401, "Invalid authorization format. Use: Bearer <token>");
        return std::nullopt;
    }
    
    auto payload = Auth::verifyToken(*token);
    if (!payload) {
        errorResponse(res, 401, "Invalid or expired token");
        return std::nullopt;
    }
    
    return payload->user_id;
}

void ApiServer::errorResponse(httplib::Response& res, int status, const std::string& message) {
    jsonResponse(res, status, {{"error", message}});
}

// Route handlers

void ApiServer::handleHealth(const httplib::Request&, httplib::Response& res) {
    jsonResponse(res, 200, {
        {"status", "healthy"},
        {"service", "gridpulse"},
        {"version", "1.0.0"}
    });
}

void ApiServer::handleGetDevices(const httplib::Request&, httplib::Response& res) {
    auto devices = db_.getAllDevices();
    nlohmann::json json_devices = nlohmann::json::array();
    for (const auto& d : devices) {
        json_devices.push_back(d);
    }
    jsonResponse(res, 200, {{"devices", json_devices}, {"count", devices.size()}});
}

void ApiServer::handleGetDevice(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("id")) {
        errorResponse(res, 400, "Missing 'id' parameter");
        return;
    }
    auto device_id = req.get_param_value("id");
    auto device = db_.getDevice(device_id);
    
    if (device) {
        jsonResponse(res, 200, *device);
    } else {
        errorResponse(res, 404, "Device not found");
    }
}

void ApiServer::handleCreateDevice(const httplib::Request& req, httplib::Response& res) {
    try {
        auto json = nlohmann::json::parse(req.body);
        
        Device device;
        device.device_id = json.at("device_id").get<std::string>();
        device.name = json.at("name").get<std::string>();
        device.location = json.value("location", "");
        device.status = "offline";
        
        // Check if device already exists
        if (db_.getDevice(device.device_id)) {
            errorResponse(res, 409, "Device already exists");
            return;
        }
        
        auto id = db_.createDevice(device);
        device.id = id;
        
        jsonResponse(res, 201, device);
    } catch (const nlohmann::json::exception& e) {
        errorResponse(res, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        errorResponse(res, 500, "Server error: " + std::string(e.what()));
    }
}

void ApiServer::handleDeleteDevice(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("id")) {
        errorResponse(res, 400, "Missing 'id' parameter");
        return;
    }
    auto device_id = req.get_param_value("id");
    
    // Check if device exists
    if (!db_.getDevice(device_id)) {
        errorResponse(res, 404, "Device not found");
        return;
    }
    
    if (db_.deleteDevice(device_id)) {
        nlohmann::json response = {{"success", true}, {"message", "Device deleted"}};
        jsonResponse(res, 200, response);
    } else {
        errorResponse(res, 500, "Failed to delete device");
    }
}

void ApiServer::handlePostTelemetry(const httplib::Request& req, httplib::Response& res) {
    try {
        auto json = nlohmann::json::parse(req.body);
        
        Telemetry telemetry;
        telemetry.device_id = json.at("device_id").get<std::string>();
        telemetry.temperature = json.at("temperature").get<double>();
        telemetry.humidity = json.at("humidity").get<double>();
        telemetry.battery_level = json.at("battery_level").get<double>();
        telemetry.timestamp = json.value("timestamp", "");
        
        // Verify device exists
        if (!db_.getDevice(telemetry.device_id)) {
            errorResponse(res, 404, "Device not found");
            return;
        }
        
        auto id = db_.insertTelemetry(telemetry);
        telemetry.id = id;
        
        // Check for alerts
        auto alerts = alert_checker_.checkTelemetry(telemetry);
        
        nlohmann::json response = telemetry;
        if (!alerts.empty()) {
            nlohmann::json alert_json = nlohmann::json::array();
            for (const auto& a : alerts) {
                alert_json.push_back(a);
            }
            response["alerts_triggered"] = alert_json;
        }
        
        jsonResponse(res, 201, response);
    } catch (const nlohmann::json::exception& e) {
        errorResponse(res, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        errorResponse(res, 500, "Server error: " + std::string(e.what()));
    }
}

void ApiServer::handleGetTelemetry(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("device_id")) {
        errorResponse(res, 400, "Missing 'device_id' parameter");
        return;
    }
    auto device_id = req.get_param_value("device_id");
    
    // Get limit from query param, default 100
    int limit = 100;
    if (req.has_param("limit")) {
        try {
            limit = std::stoi(req.get_param_value("limit"));
            limit = std::min(std::max(limit, 1), 1000);  // Clamp between 1-1000
        } catch (...) {}
    }
    
    auto telemetry = db_.getRecentTelemetry(device_id, limit);
    
    nlohmann::json json_telemetry = nlohmann::json::array();
    for (const auto& t : telemetry) {
        json_telemetry.push_back(t);
    }
    
    jsonResponse(res, 200, {
        {"device_id", device_id},
        {"telemetry", json_telemetry},
        {"count", telemetry.size()}
    });
}

void ApiServer::handleGetAlerts(const httplib::Request&, httplib::Response& res) {
    auto alerts = db_.getUnacknowledgedAlerts();
    
    nlohmann::json json_alerts = nlohmann::json::array();
    for (const auto& a : alerts) {
        json_alerts.push_back(a);
    }
    
    jsonResponse(res, 200, {{"alerts", json_alerts}, {"count", alerts.size()}});
}

void ApiServer::handleAckAlert(const httplib::Request& req, httplib::Response& res) {
    try {
        auto alert_id = std::stoll(req.matches[1].str());
        
        if (db_.acknowledgeAlert(alert_id)) {
            jsonResponse(res, 200, {{"message", "Alert acknowledged"}, {"alert_id", alert_id}});
        } else {
            errorResponse(res, 404, "Alert not found");
        }
    } catch (const std::exception& e) {
        errorResponse(res, 400, "Invalid alert ID");
    }
}

// Auth handlers

void ApiServer::handleRegister(const httplib::Request& req, httplib::Response& res) {
    try {
        auto json = nlohmann::json::parse(req.body);
        
        std::string username = json.at("username").get<std::string>();
        std::string email = json.at("email").get<std::string>();
        std::string password = json.at("password").get<std::string>();
        
        // Validate input
        if (username.length() < 3) {
            errorResponse(res, 400, "Username must be at least 3 characters");
            return;
        }
        if (password.length() < 6) {
            errorResponse(res, 400, "Password must be at least 6 characters");
            return;
        }
        if (email.find('@') == std::string::npos) {
            errorResponse(res, 400, "Invalid email address");
            return;
        }
        
        // Check if user exists
        if (db_.userExists(username, email)) {
            errorResponse(res, 409, "Username or email already registered");
            return;
        }
        
        // Hash password and create user
        std::string password_hash = Auth::hashPassword(password);
        auto user_id = db_.createUser(username, email, password_hash);
        
        // Generate token
        std::string token = Auth::generateToken(user_id, username);
        
        jsonResponse(res, 201, {
            {"message", "Registration successful"},
            {"token", token},
            {"user", {
                {"id", user_id},
                {"username", username},
                {"email", email}
            }}
        });
        
        spdlog::info("New user registered: {}", username);
        
    } catch (const nlohmann::json::exception& e) {
        errorResponse(res, 400, "Invalid JSON: username, email, and password required");
    } catch (const std::exception& e) {
        spdlog::error("Registration error: {}", e.what());
        errorResponse(res, 500, "Registration failed");
    }
}

void ApiServer::handleLogin(const httplib::Request& req, httplib::Response& res) {
    try {
        auto json = nlohmann::json::parse(req.body);
        
        std::string username = json.at("username").get<std::string>();
        std::string password = json.at("password").get<std::string>();
        
        // Find user
        auto user = db_.getUserByUsername(username);
        if (!user) {
            // Try email
            user = db_.getUserByEmail(username);
        }
        
        if (!user) {
            errorResponse(res, 401, "Invalid credentials");
            return;
        }
        
        // Verify password
        if (!Auth::verifyPassword(password, user->password_hash)) {
            errorResponse(res, 401, "Invalid credentials");
            return;
        }
        
        // Generate token
        std::string token = Auth::generateToken(user->id, user->username);
        
        jsonResponse(res, 200, {
            {"message", "Login successful"},
            {"token", token},
            {"user", {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email}
            }}
        });
        
        spdlog::info("User logged in: {}", user->username);
        
    } catch (const nlohmann::json::exception& e) {
        errorResponse(res, 400, "Invalid JSON: username and password required");
    } catch (const std::exception& e) {
        spdlog::error("Login error: {}", e.what());
        errorResponse(res, 500, "Login failed");
    }
}

void ApiServer::handleGetMe(const httplib::Request& req, httplib::Response& res) {
    auto user_id = authenticate(req, res);
    if (!user_id) return;  // authenticate already sent error response
    
    auto user = db_.getUserById(*user_id);
    if (!user) {
        errorResponse(res, 404, "User not found");
        return;
    }
    
    jsonResponse(res, 200, {
        {"id", user->id},
        {"username", user->username},
        {"email", user->email},
        {"created_at", user->created_at}
    });
}

} // namespace gridpulse
