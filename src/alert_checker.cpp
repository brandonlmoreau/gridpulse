#include "gridpulse/alert_checker.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace gridpulse {

AlertChecker::AlertChecker(Database& db, const AlertConfig& config)
    : db_(db), config_(config) {
}

std::vector<Alert> AlertChecker::checkTelemetry(const Telemetry& telemetry) {
    std::vector<Alert> new_alerts;
    
    // Check temperature threshold
    if (auto alert = checkTemperature(telemetry)) {
        db_.createAlert(*alert);
        new_alerts.push_back(*alert);
    }
    
    // Check battery threshold
    if (auto alert = checkBattery(telemetry)) {
        db_.createAlert(*alert);
        new_alerts.push_back(*alert);
    }
    
    return new_alerts;
}

std::optional<Alert> AlertChecker::checkTemperature(const Telemetry& telemetry) {
    if (telemetry.temperature > config_.max_temperature) {
        Alert alert;
        alert.device_id = telemetry.device_id;
        alert.alert_type = "high_temperature";
        alert.severity = telemetry.temperature > (config_.max_temperature + 10) ? "critical" : "warning";
        
        std::ostringstream msg;
        msg << "Temperature " << std::fixed << std::setprecision(1) 
            << telemetry.temperature << "°C exceeds threshold of " 
            << config_.max_temperature << "°C";
        alert.message = msg.str();
        
        spdlog::warn("High temperature alert for {}: {:.1f}°C", 
                    telemetry.device_id, telemetry.temperature);
        return alert;
    }
    return std::nullopt;
}

std::optional<Alert> AlertChecker::checkBattery(const Telemetry& telemetry) {
    if (telemetry.battery_level < config_.min_battery) {
        Alert alert;
        alert.device_id = telemetry.device_id;
        alert.alert_type = "low_battery";
        alert.severity = telemetry.battery_level < 10.0 ? "critical" : "warning";
        
        std::ostringstream msg;
        msg << "Battery level " << std::fixed << std::setprecision(1) 
            << telemetry.battery_level << "% below threshold of " 
            << config_.min_battery << "%";
        alert.message = msg.str();
        
        spdlog::warn("Low battery alert for {}: {:.1f}%", 
                    telemetry.device_id, telemetry.battery_level);
        return alert;
    }
    return std::nullopt;
}

void AlertChecker::setConfig(const AlertConfig& config) {
    config_ = config;
    spdlog::info("Alert config updated: max_temp={}, min_battery={}", 
                config_.max_temperature, config_.min_battery);
}

const AlertConfig& AlertChecker::getConfig() const {
    return config_;
}

} // namespace gridpulse
