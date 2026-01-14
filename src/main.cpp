#include "gridpulse/database.hpp"
#include "gridpulse/api_server.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <cstdlib>
#include <memory>

// Global pointer for signal handling
std::unique_ptr<gridpulse::ApiServer> g_server;

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    
    spdlog::info("GridPulse IoT Platform v1.0.0");
    spdlog::info("=====================================");
    
    // Parse command line arguments
    int port = 8080;
    std::string db_path = "gridpulse.db";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if ((arg == "-d" || arg == "--database") && i + 1 < argc) {
            db_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            spdlog::info("Usage: {} [options]", argv[0]);
            spdlog::info("Options:");
            spdlog::info("  -p, --port PORT       Server port (default: 8080)");
            spdlog::info("  -d, --database PATH   Database file (default: gridpulse.db)");
            spdlog::info("  -h, --help            Show this help");
            return 0;
        }
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Initialize database
        gridpulse::Database db(db_path);
        db.initialize();
        
        // Create and start server
        g_server = std::make_unique<gridpulse::ApiServer>(db, port);
        
        spdlog::info("API server starting on http://0.0.0.0:{}", port);
        spdlog::info("Press Ctrl+C to stop");
        
        g_server->run();
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    spdlog::info("Server stopped");
    return 0;
}
