/**
 * Device Simulator for GridPulse
 * 
 * Simulates IoT devices sending telemetry data to the GridPulse API.
 * Useful for testing and demonstration purposes.
 */

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <vector>
#include <string>

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

class DeviceSimulator {
public:
    DeviceSimulator(const std::string& server_url, const std::string& device_id)
        : client_(server_url), device_id_(device_id), 
          rng_(std::random_device{}()) {
        
        // Initialize with random starting values
        temperature_ = std::uniform_real_distribution<>(20.0, 30.0)(rng_);
        humidity_ = std::uniform_real_distribution<>(40.0, 60.0)(rng_);
        battery_ = std::uniform_real_distribution<>(80.0, 100.0)(rng_);
    }
    
    bool registerDevice(const std::string& name, const std::string& location) {
        nlohmann::json body = {
            {"device_id", device_id_},
            {"name", name},
            {"location", location}
        };
        
        auto res = client_.Post("/api/devices", body.dump(), "application/json");
        if (res && (res->status == 201 || res->status == 409)) {
            spdlog::info("Device {} registered", device_id_);
            return true;
        }
        
        spdlog::error("Failed to register device: {}", 
                     res ? std::to_string(res->status) : "connection failed");
        return false;
    }
    
    bool sendTelemetry() {
        // Simulate gradual changes in values
        temperature_ += std::uniform_real_distribution<>(-0.5, 0.5)(rng_);
        humidity_ += std::uniform_real_distribution<>(-1.0, 1.0)(rng_);
        battery_ -= std::uniform_real_distribution<>(0.0, 0.1)(rng_);
        
        // Clamp values to realistic ranges
        temperature_ = std::clamp(temperature_, 15.0, 50.0);
        humidity_ = std::clamp(humidity_, 20.0, 90.0);
        battery_ = std::clamp(battery_, 0.0, 100.0);
        
        nlohmann::json body = {
            {"device_id", device_id_},
            {"temperature", temperature_},
            {"humidity", humidity_},
            {"battery_level", battery_}
        };
        
        auto res = client_.Post("/api/telemetry", body.dump(), "application/json");
        if (res && res->status == 201) {
            auto response = nlohmann::json::parse(res->body);
            
            // Check if any alerts were triggered
            if (response.contains("alerts_triggered")) {
                for (const auto& alert : response["alerts_triggered"]) {
                    spdlog::warn("[{}] Alert: {}", device_id_, alert["message"].get<std::string>());
                }
            }
            return true;
        }
        
        spdlog::error("[{}] Failed to send telemetry", device_id_);
        return false;
    }
    
    void printStatus() const {
        spdlog::info("[{}] temp={:.1f}°C, humidity={:.1f}%, battery={:.1f}%",
                    device_id_, temperature_, humidity_, battery_);
    }

private:
    httplib::Client client_;
    std::string device_id_;
    std::mt19937 rng_;
    
    double temperature_;
    double humidity_;
    double battery_;
};

void printUsage(const char* prog) {
    spdlog::info("Usage: {} [options]", prog);
    spdlog::info("Options:");
    spdlog::info("  -s, --server URL    Server URL (default: http://localhost:8080)");
    spdlog::info("  -n, --num N         Number of devices to simulate (default: 5)");
    spdlog::info("  -i, --interval MS   Telemetry interval in ms (default: 5000)");
    spdlog::info("  -h, --help          Show this help");
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    
    // Parse arguments
    std::string server_url = "http://localhost:8080";
    int num_devices = 5;
    int interval_ms = 5000;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--server") && i + 1 < argc) {
            server_url = argv[++i];
        } else if ((arg == "-n" || arg == "--num") && i + 1 < argc) {
            num_devices = std::atoi(argv[++i]);
        } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_ms = std::atoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }
    
    spdlog::info("GridPulse Device Simulator");
    spdlog::info("==========================");
    spdlog::info("Server: {}", server_url);
    spdlog::info("Devices: {}", num_devices);
    spdlog::info("Interval: {}ms", interval_ms);
    
    // Setup signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create simulators
    std::vector<std::unique_ptr<DeviceSimulator>> simulators;
    std::vector<std::string> locations = {"Building A", "Building B", "Warehouse", "Office", "Lab"};
    
    for (int i = 0; i < num_devices; i++) {
        auto device_id = "sensor-" + std::to_string(i + 1).insert(0, 3 - std::to_string(i + 1).length(), '0');
        auto sim = std::make_unique<DeviceSimulator>(server_url, device_id);
        
        auto name = "Temperature Sensor " + std::to_string(i + 1);
        auto location = locations[i % locations.size()];
        
        if (sim->registerDevice(name, location)) {
            simulators.push_back(std::move(sim));
        }
    }
    
    if (simulators.empty()) {
        spdlog::error("No devices registered. Is the server running?");
        return 1;
    }
    
    spdlog::info("Simulating {} devices. Press Ctrl+C to stop.", simulators.size());
    
    // Main simulation loop
    while (g_running) {
        for (auto& sim : simulators) {
            if (!g_running) break;
            
            sim->sendTelemetry();
            sim->printStatus();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    
    spdlog::info("Simulator stopped");
    return 0;
}
