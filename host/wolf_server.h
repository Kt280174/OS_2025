#pragma once

#include "../Conn/conn.h"
#include <iostream>
#include <signal.h>
#include <vector>
#include <memory>
#include <map>
#include <condition_variable>
#include <mutex>
#include <atomic>

class Wolf {
public:
    // CONSTRUCTOR VÀ DESTRUCTOR PUBLIC
    Wolf();
    ~Wolf();

    static Wolf& instance();

    void set_max_goats_num(size_t max_goats);
    int run();

    // Delete copy constructor and assignment operator
    Wolf(const Wolf&) = delete;
    Wolf& operator=(const Wolf&) = delete;

private:
    static void signal_handler(int signal, siginfo_t* info, void* context);

    void initialize_signal_handlers();
    void wait_for_connections();
    void start_game();
    void game_loop();
    bool process_single_goat(pid_t goat_pid);
    void cleanup();

    // 3 method xử lý signal
    void handle_new_connection(pid_t client_pid);
    void handle_client_disconnection(pid_t client_pid);
    void handle_termination();

    size_t generate_wolf_number() const;
    bool chase_goat(bool goat_status, size_t goat_number) const;
    void update_game_state(bool all_goats_dead);
    size_t get_wolf_number_input();
    void display_game_status();

private:
    std::atomic<bool> game_continue_{ true };
    std::atomic<bool> all_goats_connected_{ false };
    std::atomic<size_t> wolf_number_{ 0 };

    size_t max_goats_{ 0 };
    size_t consecutive_dead_turns_{ 0 };

    std::map<pid_t, bool> goats_;
    std::vector<Connection*> connections_;

    std::mutex goats_mutex_;
    std::condition_variable connection_cv_;

    static std::unique_ptr<Wolf> instance_;
};