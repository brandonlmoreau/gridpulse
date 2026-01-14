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
    // Devices table (with user ownership)
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            device_id TEXT NOT NULL,
            name TEXT NOT NULL,
            location TEXT,
            status TEXT DEFAULT 'offline',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id),
            UNIQUE(user_id, device_id)
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
    
    // Users table
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    )");
    
    // Create indexes for common queries
    db_->exec("CREATE INDEX IF NOT EXISTS idx_devices_user ON devices(user_id)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_device ON telemetry(device_id)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp ON telemetry(timestamp)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_alerts_device ON alerts(device_id)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)");
}

std::string Database::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Device operations

int64_t Database::createDevice(const Device& device, int64_t user_id) {
    SQLite::Statement query(*db_, 
        "INSERT INTO devices (user_id, device_id, name, location, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    
    auto timestamp = getCurrentTimestamp();
    query.bind(1, user_id);
    query.bind(2, device.device_id);
    query.bind(3, device.name);
    query.bind(4, device.location);
    query.bind(5, device.status.empty() ? "offline" : device.status);
    query.bind(6, timestamp);
    query.bind(7, timestamp);
    
    query.exec();
    auto id = db_->getLastInsertRowid();
    spdlog::info("Created device: {} for user {} (id={})", device.device_id, user_id, id);
    return id;
}

std::optional<Device> Database::getDevice(const std::string& device_id, int64_t user_id) {
    SQLite::Statement query(*db_, 
        "SELECT id, user_id, device_id, name, location, status, created_at, updated_at "
        "FROM devices WHERE device_id = ? AND user_id = ?");
    query.bind(1, device_id);
    query.bind(2, user_id);
    
    if (query.executeStep()) {
        Device d;
        d.id = query.getColumn(0).getInt64();
        d.user_id = query.getColumn(1).getInt64();
        d.device_id = query.getColumn(2).getString();
        d.name = query.getColumn(3).getString();
        d.location = query.getColumn(4).getString();
        d.status = query.getColumn(5).getString();
        d.created_at = query.getColumn(6).getString();
        d.updated_at = query.getColumn(7).getString();
        return d;
    }
    return std::nullopt;
}

std::optional<Device> Database::getDeviceAny(const std::string& device_id) {
    SQLite::Statement query(*db_, 
        "SELECT id, user_id, device_id, name, location, status, created_at, updated_at "
        "FROM devices WHERE device_id = ?");
    query.bind(1, device_id);
    
    if (query.executeStep()) {
        Device d;
        d.id = query.getColumn(0).getInt64();
        d.user_id = query.getColumn(1).getInt64();
        d.device_id = query.getColumn(2).getString();
        d.name = query.getColumn(3).getString();
        d.location = query.getColumn(4).getString();
        d.status = query.getColumn(5).getString();
        d.created_at = query.getColumn(6).getString();
        d.updated_at = query.getColumn(7).getString();
        return d;
    }
    return std::nullopt;
}

bool Database::isDeviceOwner(const std::string& device_id, int64_t user_id) {
    SQLite::Statement query(*db_, 
        "SELECT COUNT(*) FROM devices WHERE device_id = ? AND user_id = ?");
    query.bind(1, device_id);
    query.bind(2, user_id);
    
    if (query.executeStep()) {
        return query.getColumn(0).getInt() > 0;
    }
    return false;
}

std::vector<Device> Database::getAllDevices(int64_t user_id) {
    std::vector<Device> devices;
    SQLite::Statement query(*db_, 
        "SELECT id, user_id, device_id, name, location, status, created_at, updated_at "
        "FROM devices WHERE user_id = ?");
    query.bind(1, user_id);
    
    while (query.executeStep()) {
        Device d;
        d.id = query.getColumn(0).getInt64();
        d.user_id = query.getColumn(1).getInt64();
        d.device_id = query.getColumn(2).getString();
        d.name = query.getColumn(3).getString();
        d.location = query.getColumn(4).getString();
        d.status = query.getColumn(5).getString();
        d.created_at = query.getColumn(6).getString();
        d.updated_at = query.getColumn(7).getString();
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

bool Database::deleteDevice(const std::string& device_id, int64_t user_id) {
    SQLite::Statement query(*db_, "DELETE FROM devices WHERE device_id = ? AND user_id = ?");
    query.bind(1, device_id);
    query.bind(2, user_id);
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

std::vector<Alert> Database::getUnacknowledgedAlerts(int64_t user_id) {
    std::vector<Alert> alerts;
    // Only get alerts for devices owned by this user
    SQLite::Statement query(*db_,
        "SELECT a.id, a.device_id, a.alert_type, a.message, a.severity, a.acknowledged, a.created_at "
        "FROM alerts a "
        "INNER JOIN devices d ON a.device_id = d.device_id "
        "WHERE a.acknowledged = 0 AND d.user_id = ? "
        "ORDER BY a.created_at DESC");
    query.bind(1, user_id);
    
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

bool Database::acknowledgeAlert(int64_t alert_id, int64_t user_id) {
    // Only allow acknowledging alerts for devices owned by this user
    SQLite::Statement query(*db_, 
        "UPDATE alerts SET acknowledged = 1 "
        "WHERE id = ? AND device_id IN (SELECT device_id FROM devices WHERE user_id = ?)");
    query.bind(1, alert_id);
    query.bind(2, user_id);
    auto updated = query.exec() > 0;
    if (updated) {
        spdlog::info("Alert {} acknowledged by user {}", alert_id, user_id);
    }
    return updated;
}

// User operations

int64_t Database::createUser(const std::string& username, const std::string& email, const std::string& password_hash) {
    SQLite::Statement query(*db_,
        "INSERT INTO users (username, email, password_hash, created_at) VALUES (?, ?, ?, ?)");
    
    query.bind(1, username);
    query.bind(2, email);
    query.bind(3, password_hash);
    query.bind(4, getCurrentTimestamp());
    
    query.exec();
    auto id = db_->getLastInsertRowid();
    spdlog::info("Created user: {} (id={})", username, id);
    return id;
}

std::optional<User> Database::getUserByUsername(const std::string& username) {
    SQLite::Statement query(*db_,
        "SELECT id, username, email, password_hash, created_at FROM users WHERE username = ?");
    query.bind(1, username);
    
    if (query.executeStep()) {
        User u;
        u.id = query.getColumn(0).getInt64();
        u.username = query.getColumn(1).getString();
        u.email = query.getColumn(2).getString();
        u.password_hash = query.getColumn(3).getString();
        u.created_at = query.getColumn(4).getString();
        return u;
    }
    return std::nullopt;
}

std::optional<User> Database::getUserByEmail(const std::string& email) {
    SQLite::Statement query(*db_,
        "SELECT id, username, email, password_hash, created_at FROM users WHERE email = ?");
    query.bind(1, email);
    
    if (query.executeStep()) {
        User u;
        u.id = query.getColumn(0).getInt64();
        u.username = query.getColumn(1).getString();
        u.email = query.getColumn(2).getString();
        u.password_hash = query.getColumn(3).getString();
        u.created_at = query.getColumn(4).getString();
        return u;
    }
    return std::nullopt;
}

std::optional<User> Database::getUserById(int64_t user_id) {
    SQLite::Statement query(*db_,
        "SELECT id, username, email, password_hash, created_at FROM users WHERE id = ?");
    query.bind(1, user_id);
    
    if (query.executeStep()) {
        User u;
        u.id = query.getColumn(0).getInt64();
        u.username = query.getColumn(1).getString();
        u.email = query.getColumn(2).getString();
        u.password_hash = query.getColumn(3).getString();
        u.created_at = query.getColumn(4).getString();
        return u;
    }
    return std::nullopt;
}

bool Database::userExists(const std::string& username, const std::string& email) {
    SQLite::Statement query(*db_,
        "SELECT COUNT(*) FROM users WHERE username = ? OR email = ?");
    query.bind(1, username);
    query.bind(2, email);
    
    if (query.executeStep()) {
        return query.getColumn(0).getInt() > 0;
    }
    return false;
}

} // namespace gridpulse
