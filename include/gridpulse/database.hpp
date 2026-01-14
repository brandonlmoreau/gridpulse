#pragma once

#include "models.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <memory>
#include <vector>
#include <optional>

namespace gridpulse {

/**
 * Database layer for GridPulse using SQLite
 * Handles all CRUD operations for devices, telemetry, and alerts
 */
class Database {
public:
    explicit Database(const std::string& db_path = "gridpulse.db");
    
    // Initialize database schema
    void initialize();
    
    // Device operations
    int64_t createDevice(const Device& device);
    std::optional<Device> getDevice(const std::string& device_id);
    std::vector<Device> getAllDevices();
    bool updateDeviceStatus(const std::string& device_id, const std::string& status);
    bool deleteDevice(const std::string& device_id);
    
    // Telemetry operations
    int64_t insertTelemetry(const Telemetry& telemetry);
    std::vector<Telemetry> getRecentTelemetry(const std::string& device_id, int limit = 100);
    std::optional<Telemetry> getLatestTelemetry(const std::string& device_id);
    
    // Alert operations
    int64_t createAlert(const Alert& alert);
    std::vector<Alert> getUnacknowledgedAlerts();
    std::vector<Alert> getAlertsByDevice(const std::string& device_id);
    bool acknowledgeAlert(int64_t alert_id);
    
    // User operations
    int64_t createUser(const std::string& username, const std::string& email, const std::string& password_hash);
    std::optional<User> getUserByUsername(const std::string& username);
    std::optional<User> getUserByEmail(const std::string& email);
    std::optional<User> getUserById(int64_t user_id);
    bool userExists(const std::string& username, const std::string& email);

private:
    std::unique_ptr<SQLite::Database> db_;
    
    void createTables();
    std::string getCurrentTimestamp();
};

} // namespace gridpulse
