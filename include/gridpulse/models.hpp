#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

namespace gridpulse {

// Represents an IoT device in the system
struct Device {
    int64_t id = 0;
    std::string device_id;      // Unique identifier (e.g., "sensor-001")
    std::string name;           // Human-readable name
    std::string location;       // Physical location
    std::string status;         // "online", "offline", "error"
    std::string created_at;
    std::string updated_at;
};

// Telemetry data point from a device
struct Telemetry {
    int64_t id = 0;
    std::string device_id;
    double temperature;         // Celsius
    double humidity;            // Percentage
    double battery_level;       // Percentage
    std::string timestamp;
};

// Alert generated when thresholds are exceeded
struct Alert {
    int64_t id = 0;
    std::string device_id;
    std::string alert_type;     // "high_temp", "low_battery", "offline"
    std::string message;
    std::string severity;       // "warning", "critical"
    bool acknowledged = false;
    std::string created_at;
};

// JSON serialization
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Device, id, device_id, name, location, status, created_at, updated_at)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Telemetry, id, device_id, temperature, humidity, battery_level, timestamp)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Alert, id, device_id, alert_type, message, severity, acknowledged, created_at)

// Alert threshold configuration
struct AlertConfig {
    double max_temperature = 40.0;      // Celsius
    double min_battery = 20.0;          // Percentage
    int offline_timeout_seconds = 300;  // 5 minutes
};

} // namespace gridpulse
