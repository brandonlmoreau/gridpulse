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
        
        // Create a test user for all tests
        test_user_id_ = db_->createUser("testuser", "test@test.com", "hashedpassword");
    }
    
    void TearDown() override {
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }
    
    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    int64_t test_user_id_ = 0;
};

TEST_F(DatabaseTest, CreateAndGetDevice) {
    Device device;
    device.device_id = "test-001";
    device.name = "Test Sensor";
    device.location = "Lab";
    device.status = "offline";
    
    auto id = db_->createDevice(device, test_user_id_);
    EXPECT_GT(id, 0);
    
    auto retrieved = db_->getDevice("test-001", test_user_id_);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->device_id, "test-001");
    EXPECT_EQ(retrieved->name, "Test Sensor");
    EXPECT_EQ(retrieved->location, "Lab");
    EXPECT_EQ(retrieved->user_id, test_user_id_);
}

TEST_F(DatabaseTest, GetNonExistentDevice) {
    auto device = db_->getDevice("nonexistent", test_user_id_);
    EXPECT_FALSE(device.has_value());
}

TEST_F(DatabaseTest, GetAllDevices) {
    Device d1, d2;
    d1.device_id = "device-001";
    d1.name = "Device 1";
    d2.device_id = "device-002";
    d2.name = "Device 2";
    
    db_->createDevice(d1, test_user_id_);
    db_->createDevice(d2, test_user_id_);
    
    auto devices = db_->getAllDevices(test_user_id_);
    EXPECT_EQ(devices.size(), 2);
}

TEST_F(DatabaseTest, UserIsolation) {
    // Create devices for test user
    Device d1;
    d1.device_id = "user1-device";
    d1.name = "User 1 Device";
    db_->createDevice(d1, test_user_id_);
    
    // Create another user
    auto user2_id = db_->createUser("testuser2", "test2@test.com", "hashedpassword");
    
    // Create device for second user
    Device d2;
    d2.device_id = "user2-device";
    d2.name = "User 2 Device";
    db_->createDevice(d2, user2_id);
    
    // Each user should only see their own devices
    auto user1_devices = db_->getAllDevices(test_user_id_);
    auto user2_devices = db_->getAllDevices(user2_id);
    
    EXPECT_EQ(user1_devices.size(), 1);
    EXPECT_EQ(user2_devices.size(), 1);
    EXPECT_EQ(user1_devices[0].device_id, "user1-device");
    EXPECT_EQ(user2_devices[0].device_id, "user2-device");
    
    // User 1 shouldn't be able to access user 2's device
    auto cross_access = db_->getDevice("user2-device", test_user_id_);
    EXPECT_FALSE(cross_access.has_value());
}

TEST_F(DatabaseTest, UpdateDeviceStatus) {
    Device device;
    device.device_id = "test-002";
    device.name = "Test";
    device.status = "offline";
    
    db_->createDevice(device, test_user_id_);
    
    bool updated = db_->updateDeviceStatus("test-002", "online");
    EXPECT_TRUE(updated);
    
    auto retrieved = db_->getDevice("test-002", test_user_id_);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, "online");
}

TEST_F(DatabaseTest, DeleteDeviceOwnership) {
    // Create device for test user
    Device device;
    device.device_id = "delete-test";
    device.name = "Delete Test";
    db_->createDevice(device, test_user_id_);
    
    // Create another user
    auto user2_id = db_->createUser("deleteuser", "delete@test.com", "hashedpassword");
    
    // User 2 should NOT be able to delete user 1's device
    bool deleted = db_->deleteDevice("delete-test", user2_id);
    EXPECT_FALSE(deleted);
    
    // Device should still exist
    auto exists = db_->getDevice("delete-test", test_user_id_);
    EXPECT_TRUE(exists.has_value());
    
    // User 1 should be able to delete their own device
    deleted = db_->deleteDevice("delete-test", test_user_id_);
    EXPECT_TRUE(deleted);
    
    exists = db_->getDevice("delete-test", test_user_id_);
    EXPECT_FALSE(exists.has_value());
}

TEST_F(DatabaseTest, InsertAndGetTelemetry) {
    // First create a device
    Device device;
    device.device_id = "tel-test";
    device.name = "Telemetry Test";
    db_->createDevice(device, test_user_id_);
    
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
    db_->createDevice(device, test_user_id_);
    
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
    // Create a device first (alerts are filtered by device ownership)
    Device device;
    device.device_id = "alert-test";
    device.name = "Alert Test Device";
    db_->createDevice(device, test_user_id_);
    
    Alert alert;
    alert.device_id = "alert-test";
    alert.alert_type = "high_temperature";
    alert.message = "Temperature exceeded threshold";
    alert.severity = "warning";
    
    auto id = db_->createAlert(alert);
    EXPECT_GT(id, 0);
    
    auto alerts = db_->getUnacknowledgedAlerts(test_user_id_);
    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].alert_type, "high_temperature");
    EXPECT_FALSE(alerts[0].acknowledged);
}

TEST_F(DatabaseTest, AlertUserIsolation) {
    // Create devices for each user
    Device d1;
    d1.device_id = "user1-alert-device";
    d1.name = "User 1 Alert Device";
    db_->createDevice(d1, test_user_id_);
    
    auto user2_id = db_->createUser("alertuser2", "alert2@test.com", "hashedpassword");
    Device d2;
    d2.device_id = "user2-alert-device";
    d2.name = "User 2 Alert Device";
    db_->createDevice(d2, user2_id);
    
    // Create alerts for each user's device
    Alert a1;
    a1.device_id = "user1-alert-device";
    a1.alert_type = "user1_alert";
    a1.message = "User 1 alert";
    a1.severity = "warning";
    db_->createAlert(a1);
    
    Alert a2;
    a2.device_id = "user2-alert-device";
    a2.alert_type = "user2_alert";
    a2.message = "User 2 alert";
    a2.severity = "warning";
    db_->createAlert(a2);
    
    // Each user should only see their own alerts
    auto user1_alerts = db_->getUnacknowledgedAlerts(test_user_id_);
    auto user2_alerts = db_->getUnacknowledgedAlerts(user2_id);
    
    EXPECT_EQ(user1_alerts.size(), 1);
    EXPECT_EQ(user2_alerts.size(), 1);
    EXPECT_EQ(user1_alerts[0].alert_type, "user1_alert");
    EXPECT_EQ(user2_alerts[0].alert_type, "user2_alert");
}

TEST_F(DatabaseTest, AcknowledgeAlert) {
    // Create device first
    Device device;
    device.device_id = "ack-test";
    device.name = "Ack Test Device";
    db_->createDevice(device, test_user_id_);
    
    Alert alert;
    alert.device_id = "ack-test";
    alert.alert_type = "low_battery";
    alert.message = "Battery low";
    alert.severity = "critical";
    
    auto id = db_->createAlert(alert);
    
    // Verify unacknowledged
    auto unacked = db_->getUnacknowledgedAlerts(test_user_id_);
    EXPECT_EQ(unacked.size(), 1);
    
    // Acknowledge
    bool success = db_->acknowledgeAlert(id, test_user_id_);
    EXPECT_TRUE(success);
    
    // Verify no longer in unacknowledged list
    unacked = db_->getUnacknowledgedAlerts(test_user_id_);
    EXPECT_EQ(unacked.size(), 0);
}

TEST_F(DatabaseTest, AcknowledgeAlertOwnership) {
    // Create device for test user
    Device device;
    device.device_id = "ack-owner-test";
    device.name = "Ack Owner Test";
    db_->createDevice(device, test_user_id_);
    
    Alert alert;
    alert.device_id = "ack-owner-test";
    alert.alert_type = "test_alert";
    alert.message = "Test";
    alert.severity = "warning";
    auto alert_id = db_->createAlert(alert);
    
    // Create another user
    auto user2_id = db_->createUser("ackuser2", "ack2@test.com", "hashedpassword");
    
    // User 2 should NOT be able to acknowledge user 1's alert
    bool success = db_->acknowledgeAlert(alert_id, user2_id);
    EXPECT_FALSE(success);
    
    // Alert should still be unacknowledged
    auto alerts = db_->getUnacknowledgedAlerts(test_user_id_);
    EXPECT_EQ(alerts.size(), 1);
}
