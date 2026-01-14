#pragma once

#include "database.hpp"
#include "alert_checker.hpp"
#include <httplib.h>
#include <memory>

namespace gridpulse {

/**
 * REST API server for the GridPulse IoT platform
 * 
 * Endpoints:
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
 *   GET  /health               - Health check endpoint
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
