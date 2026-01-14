#pragma once

#include "database.hpp"
#include "alert_checker.hpp"
#include "auth.hpp"
#include <httplib.h>
#include <memory>
#include <optional>

namespace gridpulse {

/**
 * REST API server for the GridPulse IoT platform
 * 
 * Auth Endpoints:
 *   POST /api/auth/register    - Register new user
 *   POST /api/auth/login       - Login and get JWT token
 *   GET  /api/auth/me          - Get current user info (protected)
 * 
 * Protected Endpoints (require JWT):
 *   GET  /api/devices          - List all devices
 *   GET  /api/devices/:id      - Get device by ID
 *   POST /api/devices          - Register new device
 *   
 *   POST /api/telemetry        - Submit telemetry data
 *   GET  /api/telemetry/:id    - Get telemetry for device
 *   
 *   GET  /api/alerts           - List unacknowledged alerts
 *   POST /api/alerts/:id/ack   - Acknowledge an alert
 *   
 *   GET  /health               - Health check endpoint (public)
 */
class ApiServer {
public:
    ApiServer(Database& db, int port = 8080);
    
    // Start the server (blocking)
    void run();
    
    // Stop the server
    void stop();

private:
    Database& db_;
    AlertChecker alert_checker_;
    httplib::Server server_;
    int port_;
    
    void setupRoutes();
    
    // Auth middleware - returns user_id if authenticated, nullopt otherwise
    std::optional<int> authenticate(const httplib::Request& req, httplib::Response& res);
    
    // Auth handlers
    void handleRegister(const httplib::Request& req, httplib::Response& res);
    void handleLogin(const httplib::Request& req, httplib::Response& res);
    void handleGetMe(const httplib::Request& req, httplib::Response& res);
    
    // Route handlers
    void handleGetDevices(const httplib::Request& req, httplib::Response& res);
    void handleGetDevice(const httplib::Request& req, httplib::Response& res);
    void handleCreateDevice(const httplib::Request& req, httplib::Response& res);
    void handleDeleteDevice(const httplib::Request& req, httplib::Response& res);
    void handlePostTelemetry(const httplib::Request& req, httplib::Response& res);
    void handleGetTelemetry(const httplib::Request& req, httplib::Response& res);
    void handleGetAlerts(const httplib::Request& req, httplib::Response& res);
    void handleAckAlert(const httplib::Request& req, httplib::Response& res);
    void handleHealth(const httplib::Request& req, httplib::Response& res);
    
    // Helper to send JSON response
    void jsonResponse(httplib::Response& res, int status, const nlohmann::json& data);
    void errorResponse(httplib::Response& res, int status, const std::string& message);
};

} // namespace gridpulse
