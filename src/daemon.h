#pragma once
#include "config.h"
#include <atomic>
#include <string>

class Daemon {
public:
    static Daemon& instance();                 // Singleton
    int run(const std::string& initial_conf);  // start

private:
    Daemon() = default;
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;

    static std::atomic<bool> g_reload;
    static std::atomic<bool> g_term;

    static void on_sighup(int);
    static void on_sigterm(int);

    void daemonize();
    void setup_logging();
    void setup_signals();
    void ensure_single_instance(const std::string& pid_file);
    void write_pid_file(const std::string& pid_file);

    Config cfg_;
};
