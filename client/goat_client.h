#pragma once
#include "../Conn/conn.h"
#include <memory>
#include <signal.h>
#include <atomic>

class Goat {
public:
    Goat();
    ~Goat();
    static Goat& instance();

    bool connect_to_host(pid_t host_pid);
    int run();

    // Delete copy constructor and assignment operator
    Goat(const Goat&) = delete;
    Goat& operator=(const Goat&) = delete;
private:
   

    static void signal_handler(int signal, siginfo_t* info, void* context);

    void initialize_signal_handlers();
    size_t generate_number() const;
    void cleanup();
    bool check_connection_timeout() const;

private:
    std::atomic<bool> is_alive_{ true };
    std::atomic<bool> connection_established_{ false };
    std::atomic<bool> host_ready_{ false };
    std::atomic<bool> game_continue_{ true };

    pid_t host_pid_{ 0 };
    struct timespec connection_request_time_ {};

    static std::unique_ptr<Goat> instance_;
};