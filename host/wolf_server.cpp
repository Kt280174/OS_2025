#include "wolf_server.h"
#include "../Config/game_config.h"
#include "../Config/semaphores_manager.h"
#include "conn.h"
#include <iostream>
#include <random>
#include <syslog.h>
#include <thread>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <csignal>
#include <limits>


std::unique_ptr<Wolf> Wolf::instance_ = nullptr;

Wolf::Wolf() {
    initialize_signal_handlers();
    openlog("wolf_server", LOG_PID, 0);
}

Wolf::~Wolf() {
    cleanup();
    closelog();
}

Wolf& Wolf::instance() {
    if (!instance_) {
        instance_ = std::make_unique<Wolf>();
    }
    return *instance_;
}


void Wolf::initialize_signal_handlers() {
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGUSR2, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
}

void Wolf::signal_handler(int signal, siginfo_t* info, void* context) {
    auto& wolf = instance();
    pid_t client_pid = info->si_pid;

    switch (signal) {
    case SIGUSR1:
        wolf.handle_new_connection(client_pid);
        break;

    case SIGUSR2:
        wolf.handle_client_disconnection(client_pid);
        break;

    case SIGTERM:
    case SIGINT:
        wolf.handle_termination();
        break;
    }
}

void Wolf::handle_new_connection(pid_t client_pid) {
    std::lock_guard<std::mutex> lock(goats_mutex_);

    if (goats_.count(client_pid) > 0) {
        syslog(LOG_WARNING, "Client %d already connected", client_pid);
        return;
    }

    if (goats_.size() >= max_goats_) {
        syslog(LOG_WARNING, "Maximum client limit reached: %zu", max_goats_);
        return;
    }

    // Create semaphores for communication
    std::string host_sem_name = SemaphoreManager::HOST_SEMAPHORE_PREFIX + std::to_string(client_pid);
    std::string client_sem_name = SemaphoreManager::CLIENT_SEMAPHORE_PREFIX + std::to_string(client_pid);

    sem_t* host_sem = sem_open(host_sem_name.c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (host_sem == SEM_FAILED) {
        syslog(LOG_ERR, "Failed to create host semaphore for client %d", client_pid);
        return;
    }

    sem_t* client_sem = sem_open(client_sem_name.c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (client_sem == SEM_FAILED) {
        syslog(LOG_ERR, "Failed to create client semaphore for client %d", client_pid);
        sem_close(host_sem);
        sem_unlink(host_sem_name.c_str());
        return;
    }

    sem_close(host_sem);
    sem_close(client_sem);

    // Create connection
    Connection* connection = Connection::create(client_pid, true, sizeof(SemaphoreManager::Message));
    connections_.push_back(connection);
    goats_[client_pid] = true;

    // Notify client
    kill(client_pid, SIGUSR1);

    std::cout << "Goat connected! PID: " << client_pid
        << " (" << goats_.size() << "/" << max_goats_ << ")" << std::endl;

    if (goats_.size() == max_goats_) {
        all_goats_connected_ = true;
        connection_cv_.notify_all();
        std::cout << "All goats connected! Starting game..." << std::endl;
    }
}

void Wolf::handle_client_disconnection(pid_t client_pid) {
    std::lock_guard<std::mutex> lock(goats_mutex_);
    goats_.erase(client_pid);
    std::cout << "Goat disconnected! PID: " << client_pid << std::endl;
}

void Wolf::handle_termination() {
    std::lock_guard<std::mutex> lock(goats_mutex_);
    std::cout << "Received termination signal. Ending game..." << std::endl;

    for (auto& goat : goats_) {
        kill(goat.first, SIGTERM);
    }

    game_continue_ = false;
    all_goats_connected_ = true;
    connection_cv_.notify_all();
}

void Wolf::set_max_goats_num(size_t max_goats) {
    max_goats_ = max_goats;
}

void Wolf::wait_for_connections() {
    std::cout << "Waiting for " << max_goats_ << " goats to connect..." << std::endl;

    std::unique_lock<std::mutex> lock(goats_mutex_);
    connection_cv_.wait(lock, [this]() {
        return all_goats_connected_ || !game_continue_;
        });

    if (!game_continue_) {
        std::cout << "Game terminated while waiting for connections." << std::endl;
        return;
    }
}

void Wolf::start_game() {
    std::lock_guard<std::mutex> lock(goats_mutex_);
    for (auto& goat : goats_) {
        kill(goat.first, SIGUSR2);
    }
    syslog(LOG_INFO, "Game started with %zu goats", goats_.size());
}

int Wolf::run() {
    wait_for_connections();

    if (!game_continue_) {
        return EXIT_SUCCESS;
    }

    start_game();
    game_loop();

    std::cout << "Game over!" << std::endl;
    return EXIT_SUCCESS;
}

void Wolf::game_loop() {
    while (game_continue_) {
        std::cout << "\n=== New Round ===" << std::endl;

        wolf_number_ = get_wolf_number_input();
        std::cout << "Wolf number: " << wolf_number_ << std::endl;

        std::vector<std::thread> client_threads;
        std::vector<pid_t> current_goats;
        std::vector<bool> thread_results;

        {
            std::lock_guard<std::mutex> lock(goats_mutex_);
            if (goats_.empty()) {
                std::cout << "No goats connected. Game over." << std::endl;
                break;
            }

            for (auto& goat : goats_) {
                current_goats.push_back(goat.first);
            }
            thread_results.resize(current_goats.size(), false);
        }

        // DEBUG: In ra số goat sẽ xử lý
        std::cout << "Processing " << current_goats.size() << " goats..." << std::endl;

        // Process all goats in parallel
        for (size_t i = 0; i < current_goats.size(); ++i) {
            client_threads.emplace_back([this, client_pid = current_goats[i], i, &thread_results]() {
                thread_results[i] = process_single_goat(client_pid);
                });
        }

        // Wait for all threads to complete
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // DEBUG: In kết quả từ threads
        std::cout << "Thread results: ";
        for (bool result : thread_results) {
            std::cout << result << " ";
        }
        std::cout << std::endl;

        // Update game state
        bool all_goats_dead = true;
        size_t alive_count = 0;

        {
            std::lock_guard<std::mutex> lock(goats_mutex_);
            for (auto& goat : goats_) {
                if (goat.second) {
                    all_goats_dead = false;
                    alive_count++;
                }
            }
        }

        update_game_state(all_goats_dead);
        display_game_status();

        std::cout << "Alive goats: " << alive_count << "/" << current_goats.size() << std::endl;

        if (game_continue_) {
            std::cout << "Press Enter to continue to next round..." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
}

bool Wolf::process_single_goat(pid_t goat_pid) {
    std::cout << "Processing goat PID: " << goat_pid << std::endl;

    std::string pid_str = std::to_string(goat_pid);

    // Open semaphores for communication
    std::string host_sem_name = SemaphoreManager::HOST_SEMAPHORE_PREFIX + pid_str;
    std::string client_sem_name = SemaphoreManager::CLIENT_SEMAPHORE_PREFIX + pid_str;

    sem_t* host_sem = sem_open(host_sem_name.c_str(), 0);
    sem_t* client_sem = sem_open(client_sem_name.c_str(), 0);

    if (host_sem == SEM_FAILED || client_sem == SEM_FAILED) {
        std::cout << "ERROR: Failed to open semaphores for goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to open semaphores for goat %d", goat_pid);
        return false;
    }

    // Create connection with the goat
    Connection* connection_ptr = Connection::create(goat_pid, false);
    if (!connection_ptr) {
        std::cout << "ERROR: Failed to create connection for goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to create connection for goat %d", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    std::unique_ptr<Connection> connection(connection_ptr);

    // Wait for goat to send message (signal via client semaphore)
    struct timespec timeout {};
    if (!SemaphoreManager::setup_timeout(&timeout)) {
        std::cout << "ERROR: Failed to setup timeout for goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to setup timeout for goat %d", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    std::cout << "Waiting for message from goat " << goat_pid << "..." << std::endl;

    if (sem_timedwait(client_sem, &timeout) == -1) {
        std::cout << "ERROR: Timeout waiting for message from goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Timeout waiting for goat %d", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    // Read the message from goat
    SemaphoreManager::Message message{};
    if (!connection->Read(&message, sizeof(message))) {
        std::cout << "ERROR: Failed to read message from goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to read message from goat %d", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    std::cout << "Received from goat " << goat_pid << ": number=" << message.goat_number
        << ", status=" << (message.goat_status ? "alive" : "dead");

    // Calculate if goat survives or dies
    bool new_status = chase_goat(message.goat_status, message.goat_number);
    message.goat_status = new_status;

    std::cout << " -> " << (new_status ? "alive" : "dead") << std::endl;

    // Send response back to goat
    if (!connection->Write(&message, sizeof(message))) {
        std::cout << "ERROR: Failed to send response to goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to send response to goat %d", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    // Signal goat that response is ready (via host semaphore)
    if (sem_post(host_sem) == -1) {
        std::cout << "WARNING: Failed to post host semaphore for goat " << goat_pid << std::endl;
        syslog(LOG_ERR, "Failed to post host semaphore for goat %d", goat_pid);
    }

    // Wait for goat to acknowledge reading the response
    if (!SemaphoreManager::setup_timeout(&timeout)) {
        std::cout << "WARNING: Failed to setup timeout for goat " << goat_pid << " acknowledgment" << std::endl;
        syslog(LOG_ERR, "Failed to setup timeout for goat %d acknowledgment", goat_pid);
        sem_close(host_sem);
        sem_close(client_sem);
        return new_status;
    }

    if (sem_timedwait(client_sem, &timeout) == -1) {
        std::cout << "WARNING: Timeout waiting for goat " << goat_pid << " to acknowledge" << std::endl;
        syslog(LOG_ERR, "Timeout waiting for goat %d to acknowledge", goat_pid);
    }

    // Update goat status in the main list
    {
        std::lock_guard<std::mutex> lock(goats_mutex_);
        goats_[goat_pid] = new_status;
    }

    // Cleanup
    sem_close(host_sem);
    sem_close(client_sem);

    std::cout << "Finished processing goat " << goat_pid << std::endl;

    return new_status;
}

bool Wolf::chase_goat(bool goat_status, size_t goat_number) const {
    size_t difference = std::abs(static_cast<long>(wolf_number_ - goat_number));
    size_t threshold = goat_status ?
        GameConfig::ALIVE_GOAT_THRESHOLD / max_goats_ :
        GameConfig::DEAD_GOAT_THRESHOLD / max_goats_;

    return difference <= threshold;
}

size_t Wolf::generate_wolf_number() const {
    std::random_device rd;
    std::uniform_int_distribution<size_t> dist(
        GameConfig::MIN_WOLF_NUMBER,
        GameConfig::MAX_WOLF_NUMBER
    );
    return dist(rd);
}

size_t Wolf::get_wolf_number_input() {
    std::cout << "Enter wolf number (" << GameConfig::MIN_WOLF_NUMBER
        << "-" << GameConfig::MAX_WOLF_NUMBER
        << ") or press Enter for random: ";

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        size_t number = generate_wolf_number();
        std::cout << "Using random number: " << number << std::endl;
        return number;
    }

    try {
        size_t number = std::stoul(input);
        if (number >= GameConfig::MIN_WOLF_NUMBER && number <= GameConfig::MAX_WOLF_NUMBER) {
            return number;
        }
        else {
            std::cout << "Number out of range. Using random." << std::endl;
            return generate_wolf_number();
        }
    }
    catch (const std::exception& e) {
        std::cout << "Invalid input. Using random." << std::endl;
        return generate_wolf_number();
    }
}

void Wolf::update_game_state(bool all_goats_dead) {
    if (all_goats_dead) {
        consecutive_dead_turns_++;
        std::cout << "All goats are dead! (" << consecutive_dead_turns_
            << "/" << GameConfig::MAX_CONSECUTIVE_DEAD_TURNS << " turns)" << std::endl;
    }
    else {
        consecutive_dead_turns_ = 0;
    }

    if (consecutive_dead_turns_ >= GameConfig::MAX_CONSECUTIVE_DEAD_TURNS) {
        std::cout << "Game over: All goats have been dead for too long!" << std::endl;
        game_continue_ = false;
    }
}

void Wolf::display_game_status() {
    std::lock_guard<std::mutex> lock(goats_mutex_);
    std::cout << "\nCurrent Game Status:" << std::endl;
    std::cout << "Wolf number: " << wolf_number_ << std::endl;
    std::cout << "Connected goats: " << goats_.size() << std::endl;
    std::cout << "Goat PIDs: ";
    for (const auto& goat : goats_) {
        std::cout << goat.first << "(" << (goat.second ? "alive" : "dead") << ") ";
    }
    std::cout << std::endl;
}

void Wolf::cleanup() {
    std::lock_guard<std::mutex> lock(goats_mutex_);

    // Cleanup connections
    for (auto* connection : connections_) {
        delete connection;
    }
    connections_.clear();

    // Cleanup semaphores and send termination signals
    for (auto& goat : goats_) {
        pid_t pid = goat.first;

        // Close semaphores
        sem_t* host_sem = sem_open((SemaphoreManager::HOST_SEMAPHORE_PREFIX + std::to_string(pid)).c_str(), 0);
        sem_t* client_sem = sem_open((SemaphoreManager::CLIENT_SEMAPHORE_PREFIX + std::to_string(pid)).c_str(), 0);

        if (host_sem != SEM_FAILED) {
            sem_close(host_sem);
            sem_unlink((SemaphoreManager::HOST_SEMAPHORE_PREFIX + std::to_string(pid)).c_str());
        }

        if (client_sem != SEM_FAILED) {
            sem_close(client_sem);
            sem_unlink((SemaphoreManager::CLIENT_SEMAPHORE_PREFIX + std::to_string(pid)).c_str());
        }

        // Send termination signal
        kill(pid, SIGTERM);
    }

    goats_.clear();
}