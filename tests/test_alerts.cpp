#include <gtest/gtest.h>
#include "gridpulse/database.hpp"
#include "gridpulse/alert_checker.hpp"
#include <filesystem>

using namespace gridpulse;

class AlertCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "test_alerts.db";
        std::filesystem::remove(test_db_path_);
        
        db_ = std::make_unique<Database>(test_db_path_);
        db_->initialize();
        
        // Create a test user
        test_user_id_ = db_->createUser("alerttestuser", "alerttest@test.com", "hashedpassword");
        
        // Create test device
        Device device;
        device.device_id = "alert-device";
        device.name = "Test Device";
        db_->createDevice(device, test_user_id_);
        
        // Create alert checker with custom config
        AlertConfig config;
        config.max_temperature = 35.0;
        config.min_battery = 25.0;
        
        checker_ = std::make_unique<AlertChecker>(*db_, config);
    }
    
    void TearDown() override {
        checker_.reset();
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }
    
    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<AlertChecker> checker_;
    int64_t test_user_id_ = 0;
};

TEST_F(AlertCheckerTest, NoAlertForNormalTelemetry) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 25.0;  // Normal
    t.humidity = 50.0;
    t.battery_level = 80.0;  // Normal
    
    auto alerts = checker_->checkTelemetry(t);
    EXPECT_TRUE(alerts.empty());
}

TEST_F(AlertCheckerTest, HighTemperatureAlert) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 40.0;  // Above 35.0 threshold
    t.humidity = 50.0;
    t.battery_level = 80.0;
    
    auto alerts = checker_->checkTelemetry(t);
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "high_temperature");
    EXPECT_EQ(alerts[0].severity, "warning");
}

TEST_F(AlertCheckerTest, CriticalHighTemperatureAlert) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 50.0;  // Way above threshold (> max + 10)
    t.humidity = 50.0;
    t.battery_level = 80.0;
    
    auto alerts = checker_->checkTelemetry(t);
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "high_temperature");
    EXPECT_EQ(alerts[0].severity, "critical");
}

TEST_F(AlertCheckerTest, LowBatteryAlert) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 25.0;
    t.humidity = 50.0;
    t.battery_level = 15.0;  // Below 25% threshold
    
    auto alerts = checker_->checkTelemetry(t);
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "low_battery");
    EXPECT_EQ(alerts[0].severity, "warning");
}

TEST_F(AlertCheckerTest, CriticalLowBatteryAlert) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 25.0;
    t.humidity = 50.0;
    t.battery_level = 5.0;  // Below 10%
    
    auto alerts = checker_->checkTelemetry(t);
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "low_battery");
    EXPECT_EQ(alerts[0].severity, "critical");
}

TEST_F(AlertCheckerTest, MultipleAlerts) {
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 45.0;  // High temp
    t.humidity = 50.0;
    t.battery_level = 5.0;  // Critical low battery
    
    auto alerts = checker_->checkTelemetry(t);
    EXPECT_EQ(alerts.size(), 2);
    
    // Check we have both alert types
    bool has_temp = false, has_battery = false;
    for (const auto& alert : alerts) {
        if (alert.alert_type == "high_temperature") has_temp = true;
        if (alert.alert_type == "low_battery") has_battery = true;
    }
    EXPECT_TRUE(has_temp);
    EXPECT_TRUE(has_battery);
}

TEST_F(AlertCheckerTest, ConfigUpdate) {
    AlertConfig new_config;
    new_config.max_temperature = 50.0;  // Raise threshold
    new_config.min_battery = 10.0;      // Lower threshold
    
    checker_->setConfig(new_config);
    
    Telemetry t;
    t.device_id = "alert-device";
    t.temperature = 45.0;  // Was alerting, now normal
    t.humidity = 50.0;
    t.battery_level = 15.0;  // Was alerting, now normal
    
    auto alerts = checker_->checkTelemetry(t);
    EXPECT_TRUE(alerts.empty());
}
