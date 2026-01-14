#include <gtest/gtest.h>
#include "gridpulse/database.hpp"
#include <filesystem>

using namespace gridpulse;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a test database file
        test_db_path_ = "test_gridpulse.db";
        // Remove if exists from previous test
        std::filesystem::remove(test_db_path_);
        
        db_ = std::make_unique<Database>(test_db_path_);
        db_->initialize();
    }
    
    void TearDown() override {
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }
    
    std::string test_db_path_;
    std::unique_ptr<Database> db_;
};

TEST_F(DatabaseTest, CreateAndGetDevice) {
    Device device;
    device.device_id = "test-001";
    device.name = "Test Sensor";
    device.location = "Lab";
    device.status = "offline";
    
    auto id = db_->createDevice(device);
    EXPECT_GT(id, 0);
    
    auto retrieved = db_->getDevice("test-001");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->device_id, "test-001");
    EXPECT_EQ(retrieved->name, "Test Sensor");
    EXPECT_EQ(retrieved->location, "Lab");
}

TEST_F(DatabaseTest, GetNonExistentDevice) {
    auto device = db_->getDevice("nonexistent");
    EXPECT_FALSE(device.has_value());
}

TEST_F(DatabaseTest, GetAllDevices) {
    Device d1, d2;
    d1.device_id = "device-001";
    d1.name = "Device 1";
    d2.device_id = "device-002";
    d2.name = "Device 2";
    
    db_->createDevice(d1);
    db_->createDevice(d2);
    
    auto devices = db_->getAllDevices();
    EXPECT_EQ(devices.size(), 2);
}

TEST_F(DatabaseTest, UpdateDeviceStatus) {
    Device device;
    device.device_id = "test-002";
    device.name = "Test";
    device.status = "offline";
    
    db_->createDevice(device);
    
    bool updated = db_->updateDeviceStatus("test-002", "online");
    EXPECT_TRUE(updated);
    
    auto retrieved = db_->getDevice("test-002");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, "online");
}

TEST_F(DatabaseTest, InsertAndGetTelemetry) {
    // First create a device
    Device device;
    device.device_id = "tel-test";
    device.name = "Telemetry Test";
    db_->createDevice(device);
    
    // Insert telemetry
    Telemetry t;
    t.device_id = "tel-test";
    t.temperature = 25.5;
    t.humidity = 60.0;
    t.battery_level = 85.0;
    
    auto id = db_->insertTelemetry(t);
    EXPECT_GT(id, 0);
    
    // Retrieve telemetry
    auto telemetry = db_->getRecentTelemetry("tel-test", 10);
    ASSERT_EQ(telemetry.size(), 1);
    EXPECT_DOUBLE_EQ(telemetry[0].temperature, 25.5);
    EXPECT_DOUBLE_EQ(telemetry[0].humidity, 60.0);
}

TEST_F(DatabaseTest, GetLatestTelemetry) {
    Device device;
    device.device_id = "latest-test";
    device.name = "Latest Test";
    db_->createDevice(device);
    
    // Insert multiple telemetry points
    for (int i = 0; i < 5; i++) {
        Telemetry t;
        t.device_id = "latest-test";
        t.temperature = 20.0 + i;
        t.humidity = 50.0;
        t.battery_level = 90.0 - i;
        db_->insertTelemetry(t);
    }
    
    auto latest = db_->getLatestTelemetry("latest-test");
    ASSERT_TRUE(latest.has_value());
    // Latest should be the last one inserted (temp = 24)
    EXPECT_DOUBLE_EQ(latest->temperature, 24.0);
}

TEST_F(DatabaseTest, CreateAndGetAlerts) {
    Alert alert;
    alert.device_id = "alert-test";
    alert.alert_type = "high_temperature";
    alert.message = "Temperature exceeded threshold";
    alert.severity = "warning";
    
    auto id = db_->createAlert(alert);
    EXPECT_GT(id, 0);
    
    auto alerts = db_->getUnacknowledgedAlerts();
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "high_temperature");
    EXPECT_FALSE(alerts[0].acknowledged);
}

TEST_F(DatabaseTest, AcknowledgeAlert) {
    Alert alert;
    alert.device_id = "ack-test";
    alert.alert_type = "low_battery";
    alert.message = "Battery low";
    alert.severity = "critical";
    
    auto id = db_->createAlert(alert);
    
    // Verify unacknowledged
    auto unacked = db_->getUnacknowledgedAlerts();
    EXPECT_EQ(unacked.size(), 1);
    
    // Acknowledge
    bool success = db_->acknowledgeAlert(id);
    EXPECT_TRUE(success);
    
    // Verify no longer in unacknowledged list
    unacked = db_->getUnacknowledgedAlerts();
    EXPECT_EQ(unacked.size(), 0);
}
