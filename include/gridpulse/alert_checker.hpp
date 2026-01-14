#pragma once

#include "models.hpp"
#include "database.hpp"
#include <functional>

namespace gridpulse {

/**
 * Checks telemetry data against thresholds and generates alerts
 */
class AlertChecker {
public:
    explicit AlertChecker(Database& db, const AlertConfig& config = AlertConfig{});
    
    // Check telemetry and create alerts if thresholds exceeded
    // Returns list of newly created alerts
    std::vector<Alert> checkTelemetry(const Telemetry& telemetry);
    
    // Update configuration
    void setConfig(const AlertConfig& config);
    const AlertConfig& getConfig() const;

private:
    Database& db_;
    AlertConfig config_;
    
    std::optional<Alert> checkTemperature(const Telemetry& telemetry);
    std::optional<Alert> checkBattery(const Telemetry& telemetry);
};

} // namespace gridpulse
