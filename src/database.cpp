#include "gridpulse/database.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace gridpulse {

Database::Database(const std::string& db_path) 
    : db_(std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    spdlog::info("Opening database: {}", db_path);
}

void Database::initialize() {
    createTables();
    spdlog::info("Database initialized successfully");
}

void Database::createTables() {
    // Devices table
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            location TEXT,
            status TEXT DEFAULT 'offline',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        )
    )");
    
    // Telemetry table
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            temperature REAL,
            humidity REAL,
            battery_level REAL,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        )
    )");
    
    // Alerts table
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            alert_type TEXT NOT NULL,
            message TEXT,
            severity TEXT DEFAULT 'warning',
            acknowledged INTEGER DEFAULT 0,
            created_at TEXT NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        )
    )");
    
    // Create indexes for common queries
    db_->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_device ON telemetry(device_id)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp ON telemetry(timestamp)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_alerts_device ON alerts(device_id)");
}

std::string Database::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Device operations

int64_t Database::createDevice(const Device& device) {
    SQLite::Statement query(*db_, 
        "INSERT INTO devices (device_id, name, location, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    
    auto timestamp = getCurrentTimestamp();
    query.bind(1, device.device_id);
    query.bind(2, device.name);
    query.bind(3, device.location);
    query.bind(4, device.status.empty() ? "offline" : device.status);
    query.bind(5, timestamp);
    query.bind(6, timestamp);
    
    query.exec();
    auto id = db_->getLastInsertRowid();
    spdlog::info("Created device: {} (id={})", device.device_id, id);
    return id;
}

std::optional<Device> Database::getDevice(const std::string& device_id) {
    SQLite::Statement query(*db_, 
        "SELECT id, device_id, name, location, status, created_at, updated_at "
        "FROM devices WHERE device_id = ?");
    query.bind(1, device_id);
    
    if (query.executeStep()) {
        Device d;
        d.id = query.getColumn(0).getInt64();
        d.device_id = query.getColumn(1).getString();
        d.name = query.getColumn(2).getString();
        d.location = query.getColumn(3).getString();
        d.status = query.getColumn(4).getString();
        d.created_at = query.getColumn(5).getString();
        d.updated_at = query.getColumn(6).getString();
        return d;
    }
    return std::nullopt;
}

std::vector<Device> Database::getAllDevices() {
    std::vector<Device> devices;
    SQLite::Statement query(*db_, 
        "SELECT id, device_id, name, location, status, created_at, updated_at FROM devices");
    
    while (query.executeStep()) {
        Device d;
        d.id = query.getColumn(0).getInt64();
        d.device_id = query.getColumn(1).getString();
        d.name = query.getColumn(2).getString();
        d.location = query.getColumn(3).getString();
        d.status = query.getColumn(4).getString();
        d.created_at = query.getColumn(5).getString();
        d.updated_at = query.getColumn(6).getString();
        devices.push_back(d);
    }
    return devices;
}

bool Database::updateDeviceStatus(const std::string& device_id, const std::string& status) {
    SQLite::Statement query(*db_, 
        "UPDATE devices SET status = ?, updated_at = ? WHERE device_id = ?");
    query.bind(1, status);
    query.bind(2, getCurrentTimestamp());
    query.bind(3, device_id);
    return query.exec() > 0;
}

bool Database::deleteDevice(const std::string& device_id) {
    SQLite::Statement query(*db_, "DELETE FROM devices WHERE device_id = ?");
    query.bind(1, device_id);
    return query.exec() > 0;
}

// Telemetry operations

int64_t Database::insertTelemetry(const Telemetry& telemetry) {
    SQLite::Statement query(*db_,
        "INSERT INTO telemetry (device_id, temperature, humidity, battery_level, timestamp) "
        "VALUES (?, ?, ?, ?, ?)");
    
    query.bind(1, telemetry.device_id);
    query.bind(2, telemetry.temperature);
    query.bind(3, telemetry.humidity);
    query.bind(4, telemetry.battery_level);
    query.bind(5, telemetry.timestamp.empty() ? getCurrentTimestamp() : telemetry.timestamp);
    
    query.exec();
    
    // Update device status to online
    updateDeviceStatus(telemetry.device_id, "online");
    
    return db_->getLastInsertRowid();
}

std::vector<Telemetry> Database::getRecentTelemetry(const std::string& device_id, int limit) {
    std::vector<Telemetry> results;
    SQLite::Statement query(*db_,
        "SELECT id, device_id, temperature, humidity, battery_level, timestamp "
        "FROM telemetry WHERE device_id = ? ORDER BY timestamp DESC LIMIT ?");
    
    query.bind(1, device_id);
    query.bind(2, limit);
    
    while (query.executeStep()) {
        Telemetry t;
        t.id = query.getColumn(0).getInt64();
        t.device_id = query.getColumn(1).getString();
        t.temperature = query.getColumn(2).getDouble();
        t.humidity = query.getColumn(3).getDouble();
        t.battery_level = query.getColumn(4).getDouble();
        t.timestamp = query.getColumn(5).getString();
        results.push_back(t);
    }
    return results;
}

std::optional<Telemetry> Database::getLatestTelemetry(const std::string& device_id) {
    auto results = getRecentTelemetry(device_id, 1);
    if (!results.empty()) {
        return results[0];
    }
    return std::nullopt;
}

// Alert operations

int64_t Database::createAlert(const Alert& alert) {
    SQLite::Statement query(*db_,
        "INSERT INTO alerts (device_id, alert_type, message, severity, acknowledged, created_at) "
        "VALUES (?, ?, ?, ?, 0, ?)");
    
    query.bind(1, alert.device_id);
    query.bind(2, alert.alert_type);
    query.bind(3, alert.message);
    query.bind(4, alert.severity);
    query.bind(5, getCurrentTimestamp());
    
    query.exec();
    auto id = db_->getLastInsertRowid();
    spdlog::warn("Alert created: {} - {} (id={})", alert.device_id, alert.alert_type, id);
    return id;
}

std::vector<Alert> Database::getUnacknowledgedAlerts() {
    std::vector<Alert> alerts;
    SQLite::Statement query(*db_,
        "SELECT id, device_id, alert_type, message, severity, acknowledged, created_at "
        "FROM alerts WHERE acknowledged = 0 ORDER BY created_at DESC");
    
    while (query.executeStep()) {
        Alert a;
        a.id = query.getColumn(0).getInt64();
        a.device_id = query.getColumn(1).getString();
        a.alert_type = query.getColumn(2).getString();
        a.message = query.getColumn(3).getString();
        a.severity = query.getColumn(4).getString();
        a.acknowledged = query.getColumn(5).getInt() != 0;
        a.created_at = query.getColumn(6).getString();
        alerts.push_back(a);
    }
    return alerts;
}

std::vector<Alert> Database::getAlertsByDevice(const std::string& device_id) {
    std::vector<Alert> alerts;
    SQLite::Statement query(*db_,
        "SELECT id, device_id, alert_type, message, severity, acknowledged, created_at "
        "FROM alerts WHERE device_id = ? ORDER BY created_at DESC LIMIT 50");
    
    query.bind(1, device_id);
    
    while (query.executeStep()) {
        Alert a;
        a.id = query.getColumn(0).getInt64();
        a.device_id = query.getColumn(1).getString();
        a.alert_type = query.getColumn(2).getString();
        a.message = query.getColumn(3).getString();
        a.severity = query.getColumn(4).getString();
        a.acknowledged = query.getColumn(5).getInt() != 0;
        a.created_at = query.getColumn(6).getString();
        alerts.push_back(a);
    }
    return alerts;
}

bool Database::acknowledgeAlert(int64_t alert_id) {
    SQLite::Statement query(*db_, "UPDATE alerts SET acknowledged = 1 WHERE id = ?");
    query.bind(1, alert_id);
    auto updated = query.exec() > 0;
    if (updated) {
        spdlog::info("Alert {} acknowledged", alert_id);
    }
    return updated;
}

} // namespace gridpulse
