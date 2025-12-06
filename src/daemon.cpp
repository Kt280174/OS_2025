#include "daemon.h"
#include "worker.h"
#include "utils.h"

#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>    
#include <cerrno> 
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace std::chrono_literals;

std::atomic<bool> Daemon::g_reload{ false };
std::atomic<bool> Daemon::g_term{ false };

Daemon& Daemon::instance() {
    static Daemon d;
    return d;
}

void Daemon::on_sighup(int) { g_reload.store(true); }
void Daemon::on_sigterm(int) { g_term.store(true); }

void Daemon::daemonize() {
    pid_t pid = fork();
    if (pid < 0) _exit(EXIT_FAILURE);
    if (pid > 0) _exit(EXIT_SUCCESS);

    if (setsid() < 0) _exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) _exit(EXIT_FAILURE);
    if (pid > 0) _exit(EXIT_SUCCESS);

    umask(0);
    if (chdir("/") != 0) {
        syslog(LOG_ERR, "Failed to change working directory to root: %s", strerror(errno));
    }


    long maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd < 0) {
        maxfd = 1024;
    }
    for (long fd = 0; fd < maxfd; ++fd) close(static_cast<int>(fd));

    int fd0 = open("/dev/null", O_RDWR);
    if (dup(fd0) < 0 || dup(fd0) < 0) {
        syslog(LOG_ERR, "Failed to duplicate file descriptors: %s", strerror(errno));
    }
    (void)fd0;
}

void Daemon::setup_logging() {
    openlog("mydaemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon starting up");
}

void Daemon::setup_signals() {
    struct sigaction sa {};
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = &Daemon::on_sighup;
    sigaction(SIGHUP, &sa, nullptr);

    sa.sa_handler = &Daemon::on_sigterm;
    sigaction(SIGTERM, &sa, nullptr);

    signal(SIGPIPE, SIG_IGN);
}

void Daemon::ensure_single_instance(const std::string& pid_file) {
    if (file_exists(pid_file)) {
        std::ifstream in(pid_file);
        pid_t oldpid = 0;
        if (in.is_open()) in >> oldpid;

        if (proc_pid_exists(oldpid)) {
            syslog(LOG_WARNING, "Found running pid=%d via /proc, sending SIGTERM", oldpid);
            kill(oldpid, SIGTERM);
            for (int i = 0; i < 100; ++i) {
                if (!proc_pid_exists(oldpid)) break;
                std::this_thread::sleep_for(50ms);
            }
        }
        unlink(pid_file.c_str());
    }
}

void Daemon::write_pid_file(const std::string& pid_file) {
    std::ofstream out(pid_file, std::ios::trunc);
    if (!out.is_open()) {
        syslog(LOG_ERR, "Cannot write pid file: %s", pid_file.c_str());
        _exit(EXIT_FAILURE);
    }
    out << getpid() << "\n";
}

int Daemon::run(const std::string& initial_conf) {
    cfg_ = Config::load_from_file(initial_conf);
    syslog(LOG_INFO, "Config loaded: %zu entries", cfg_.entries.size());
    daemonize();
    std::this_thread::sleep_for(1s);
    setup_logging();

    ensure_single_instance(cfg_.pid_file);
    write_pid_file(cfg_.pid_file);
    setup_signals();

    syslog(LOG_INFO, "Daemon started. Config: %s", cfg_.abs_path.c_str());

    while (!g_term.load()) {
        if (g_reload.load()) {
            try {
                Config re = Config::load_from_file(cfg_.abs_path, cfg_.abs_path);
                cfg_ = re;
                syslog(LOG_INFO, "Config reloaded");
            }
            catch (const std::exception& e) {
                syslog(LOG_ERR, "Config reload failed: %s", e.what());
            }
            g_reload.store(false);
        }

        try {
            Worker w(cfg_);
            w.do_work();
        }
        catch (const std::exception& e) {
            syslog(LOG_ERR, "Worker error: %s", e.what());
        }

        for (int i = 0; i < cfg_.interval && !g_term.load(); ++i)
            std::this_thread::sleep_for(1s);
    }

    syslog(LOG_INFO, "Daemon exiting");
    unlink(cfg_.pid_file.c_str());
    closelog();
    return 0;
}
