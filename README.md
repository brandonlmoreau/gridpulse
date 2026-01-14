# GridPulse - IoT Telemetry Platform

A lightweight IoT telemetry and monitoring platform built with modern C++17. GridPulse allows you to collect sensor data from IoT devices, store it in a database, and automatically generate alerts when thresholds are exceeded.

## Features

- **REST API** for device management and telemetry ingestion
- **SQLite database** for persistent storage (embedded, no external DB needed)
- **Automatic alerting** when temperature or battery thresholds are exceeded
- **Device simulator** for testing and demonstrations
- **Unit tests** with Google Test

## Architecture

```
┌─────────────────┐     HTTP POST      ┌──────────────────┐
│ IoT Devices /   │ ─────────────────► │  GridPulse API   │
│ Simulator       │     telemetry      │    (C++17)       │
└─────────────────┘                    └────────┬─────────┘
                                                │
                                                ▼
                                       ┌──────────────────┐
                                       │    SQLite DB     │
                                       │  - devices       │
                                       │  - telemetry     │
                                       │  - alerts        │
                                       └──────────────────┘
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| GET | `/api/devices` | List all devices |
| GET | `/api/devices/:id` | Get device by ID |
| POST | `/api/devices` | Register new device |
| POST | `/api/telemetry` | Submit telemetry data |
| GET | `/api/telemetry/:id` | Get device telemetry |
| GET | `/api/alerts` | List unacknowledged alerts |
| POST | `/api/alerts/:id/ack` | Acknowledge an alert |

## Building

### Prerequisites

- CMake 3.16+
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- Git

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/gridpulse.git
cd gridpulse

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .

# Run tests
ctest --output-on-failure
```

### Docker

```bash
# Build and run with Docker
docker compose up --build

# Or build manually
docker build -t gridpulse .
docker run -p 8080:8080 gridpulse
```

## Usage

### Start the Server

```bash
./build/gridpulse_server --port 8080
```

### Run the Simulator

```bash
# In another terminal, start the simulator
./build/gridpulse_simulator --server http://localhost:8080 --num 5
```

### API Examples

Register a device:
```bash
curl -X POST http://localhost:8080/api/devices \
  -H "Content-Type: application/json" \
  -d '{"device_id": "sensor-001", "name": "Office Sensor", "location": "Building A"}'
```

Submit telemetry:
```bash
curl -X POST http://localhost:8080/api/telemetry \
  -H "Content-Type: application/json" \
  -d '{"device_id": "sensor-001", "temperature": 25.5, "humidity": 60.0, "battery_level": 85.0}'
```

Get alerts:
```bash
curl http://localhost:8080/api/alerts
```

## Project Structure

```
gridpulse/
├── CMakeLists.txt          # Build configuration
├── Dockerfile              # Container build
├── include/
│   └── gridpulse/
│       ├── models.hpp      # Data structures
│       ├── database.hpp    # Database layer
│       ├── api_server.hpp  # REST API
│       └── alert_checker.hpp
├── src/
│   ├── main.cpp           # Server entry point
│   ├── database.cpp
│   ├── api_server.cpp
│   └── alert_checker.cpp
├── tools/
│   └── simulator/         # Device simulator
└── tests/                 # Unit tests
```

## Technologies Used

- **C++17** - Modern C++ features (std::optional, structured bindings, etc.)
- **cpp-httplib** - Header-only HTTP server library
- **nlohmann/json** - JSON parsing and serialization
- **SQLiteCpp** - SQLite wrapper for C++
- **spdlog** - Fast logging library
- **Google Test** - Unit testing framework
- **CMake** - Build system with FetchContent for dependencies

## License

MIT License - see [LICENSE](LICENSE) file.
