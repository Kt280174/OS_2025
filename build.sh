#!/usr/bin/env bash
# =====================================================
#  Build & Control Script for MyDaemon
# =====================================================

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BIN_NAME="mydaemon"
EXECUTABLE="$BUILD_DIR/$BIN_NAME"
CONFIG_FILE="$ROOT_DIR/config.txt"
PID_FILE="/tmp/mydaemon.pid"

function build_daemon() {
    echo "Cleaning old build directory..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    echo "Configuring with CMake..."
    cd "$BUILD_DIR"
    cmake ..

    echo "Building..."
    cmake --build . -- -j$(nproc)

    if [[ -f "$EXECUTABLE" ]]; then
        echo
        echo "Build successful!"
        echo "Executable: $EXECUTABLE"
    else
        echo "Build failed!"
        exit 1
    fi
}

function run_daemon() {
    if [[ -f "$PID_FILE" ]]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" &>/dev/null; then
            echo "Daemon is already running with PID=$PID."
            echo "Use './build.sh --reload' to reload or './build.sh --stop' to stop."
            return
        fi
    fi
    echo "Starting daemon..."
    nohup "$EXECUTABLE" "$CONFIG_FILE" >/dev/null 2>&1 &
    sleep 1
    if [[ -f "$PID_FILE" ]]; then
        echo "Daemon started with PID=$(cat $PID_FILE)"
    else
        echo "Daemon started, but PID file not found (check syslog)."
    fi
}

function reload_daemon() {
    if [[ -f "$PID_FILE" ]]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" &>/dev/null; then
            echo "Sending SIGHUP to PID=$PID (reload config)..."
            kill -HUP "$PID"
            echo "Config reload signal sent."
        else
            echo "No running daemon found with PID=$PID"
        fi
    else
        echo "PID file not found ($PID_FILE)"
    fi
}

function stop_daemon() {
    if [[ -f "$PID_FILE" ]]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" &>/dev/null; then
            echo "Sending SIGTERM to PID=$PID..."
            kill -TERM "$PID"
            echo "Stop signal sent."
        else
            echo "No process found with PID=$PID"
        fi
        rm -f "$PID_FILE"
    else
        echo "PID file not found ($PID_FILE)"
    fi
}

# === main ===
ACTION="${1:-build}"

case "$ACTION" in
    --run)
        build_daemon
        run_daemon
        ;;
    --reload)
        reload_daemon
        ;;
    --stop)
        stop_daemon
        ;;
    *)
        build_daemon
        echo
        echo "To run daemon:   ./build.sh --run"
        echo "To reload:       ./build.sh --reload"
        echo "To stop:         ./build.sh --stop"
        ;;
esac
