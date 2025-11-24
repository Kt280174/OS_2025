#include "goat_client.h"
#include "../Config/game_config.h"
#include "../Config/semaphores_manager.h"
#include <iostream>
#include <random>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

std::unique_ptr<Goat> Goat::instance_ = nullptr;

Goat::Goat() {
    initialize_signal_handlers();
    openlog("goat_client", LOG_PID, 0);
}

Goat::~Goat() {
    closelog();
}

Goat& Goat::instance() {
    if (!instance_) {
        instance_ = std::make_unique<Goat>();
    }
    return *instance_;
}

void Goat::initialize_signal_handlers() {
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGUSR2, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
}

void Goat::signal_handler(int signal, siginfo_t* info, void* context) {
    auto& goat = instance();

    switch (signal) {
    case SIGUSR1:
        goat.connection_established_ = true;
        syslog(LOG_INFO, "Connection established with host");
        break;

    case SIGUSR2:
        goat.host_ready_ = true;
        syslog(LOG_INFO, "Host ready - starting game");
        break;

    case SIGTERM:
    case SIGINT:
        goat.game_continue_ = false;
        syslog(LOG_INFO, "Received termination signal");
        break;
    }
}

bool Goat::connect_to_host(pid_t host_pid) {
    syslog(LOG_INFO, "Connecting to host PID: %d", host_pid);

    // Verify host process exists
    struct stat process_stat {};
    if (stat(("/proc/" + std::to_string(host_pid)).c_str(), &process_stat) == -1) {
        syslog(LOG_ERR, "Host process %d does not exist", host_pid);
        return false;
    }

    host_pid_ = host_pid;
    kill(host_pid_, SIGUSR1);

    if (clock_gettime(CLOCK_REALTIME, &connection_request_time_) == -1) {
        syslog(LOG_ERR, "Failed to get connection request time");
        return false;
    }

    return true;
}

int Goat::run() {
    // Wait for connection establishment
    while (!connection_established_ && game_continue_) {
        if (check_connection_timeout()) {
            syslog(LOG_ERR, "Connection timeout");
            return EXIT_FAILURE;
        }
        usleep(100000); // 100ms
    }

    // Wait for host readiness
    while (!host_ready_ && game_continue_) {
        usleep(100000); // 100ms
    }

    pid_t client_pid = getpid();
    syslog(LOG_INFO, "Client %d starting game loop", client_pid);

    // Game loop
    while (game_continue_) {
        std::cout << "DEBUG: Client " << client_pid << " creating connection..." << std::endl; // DEBUG

        Connection* connection_ptr = Connection::create(client_pid, false);
        if (!connection_ptr) {
            syslog(LOG_ERR, "Failed to create connection");
            break;
        }

        std::unique_ptr<Connection> connection(connection_ptr);

        // Send goat status and number
        SemaphoreManager::Message message{ is_alive_.load(), generate_number() };
        std::cout << "DEBUG: Client " << client_pid << " sending message - status: "
            << message.goat_status << ", number: " << message.goat_number << std::endl; // DEBUG

        if (!connection->Write(&message, sizeof(message))) {
            syslog(LOG_ERR, "Failed to send message to host");
            break;
        }

        syslog(LOG_INFO, "Client %d sent: status=%d, number=%zu",
            client_pid, message.goat_status, message.goat_number);

        // DEBUG: Open and post client semaphore
        std::string client_sem_name = SemaphoreManager::CLIENT_SEMAPHORE_PREFIX + std::to_string(client_pid);
        sem_t* client_sem = sem_open(client_sem_name.c_str(), 0);
        if (client_sem == SEM_FAILED) {
            std::cout << "DEBUG: Client " << client_pid << " failed to open client semaphore" << std::endl;
        }
        else {
            std::cout << "DEBUG: Client " << client_pid << " posting client semaphore" << std::endl;
            if (sem_post(client_sem) == -1) {
                std::cout << "DEBUG: Client " << client_pid << " failed to post client semaphore" << std::endl;
            }
            sem_close(client_sem);
        }

        // Wait for host response
        std::string host_sem_name = SemaphoreManager::HOST_SEMAPHORE_PREFIX + std::to_string(client_pid);
        sem_t* host_sem = sem_open(host_sem_name.c_str(), 0);
        if (host_sem == SEM_FAILED) {
            std::cout << "DEBUG: Client " << client_pid << " failed to open host semaphore" << std::endl;
        }
        else {
            std::cout << "DEBUG: Client " << client_pid << " waiting for host semaphore..." << std::endl;
            struct timespec timeout {};
            if (SemaphoreManager::setup_timeout(&timeout)) {
                if (sem_timedwait(host_sem, &timeout) == -1) {
                    std::cout << "DEBUG: Client " << client_pid << " timeout waiting for host" << std::endl;
                }
                else {
                    std::cout << "DEBUG: Client " << client_pid << " received host semaphore" << std::endl;
                }
            }
            sem_close(host_sem);
        }

        SemaphoreManager::Message response{};
        if (!connection->Read(&response, sizeof(response))) {
            syslog(LOG_ERR, "Failed to receive response from host");
            break;
        }

        is_alive_ = response.goat_status;
        syslog(LOG_INFO, "Client %d updated status: %d", client_pid, is_alive_.load());

        // Post client semaphore again to signal we read the response
        client_sem = sem_open(client_sem_name.c_str(), 0);
        if (client_sem != SEM_FAILED) {
            sem_post(client_sem);
            sem_close(client_sem);
        }

        std::cout << "DEBUG: Client " << client_pid << " finished round" << std::endl;
    }

    cleanup();
    syslog(LOG_INFO, "Client %d finished execution", client_pid);
    return EXIT_SUCCESS;
}
size_t Goat::generate_number() const {
    size_t max_number = is_alive_ ? GameConfig::MAX_ALIVE_GOAT_NUMBER
        : GameConfig::MAX_DEAD_GOAT_NUMBER;

    std::random_device rd;
    std::uniform_int_distribution<size_t> dist(GameConfig::MIN_GOAT_NUMBER, max_number);
    return dist(rd);
}

void Goat::cleanup() {
    if (host_pid_ > 0) {
        kill(host_pid_, SIGUSR2);
    }
}

bool Goat::check_connection_timeout() const {
    struct timespec current_time {};
    if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
        return true; // Consider it timeout if we can't get time
    }

    time_t time_diff = current_time.tv_sec - connection_request_time_.tv_sec;
    return time_diff > static_cast<time_t>(GameConfig::TIMEOUT_SECONDS);
}