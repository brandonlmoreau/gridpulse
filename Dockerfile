# Simple single-stage build for GridPulse
FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel 2

# Copy public folder to build directory for static file serving
RUN cp -r /app/public /app/build/public

# Expose the API port
EXPOSE 8080

# Run the server
WORKDIR /app/build
CMD ["./gridpulse_server", "--port", "8080"]
