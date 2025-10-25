#pragma once
#include <string>
#include <vector>

struct Task {
    std::string src;
    std::string dst;
    std::string ext;
    std::string subfolder;
};

struct Config {
    std::string abs_path;                
    std::string pid_file = "/tmp/mydaemon.pid";
    int interval = 60;                    // second
    std::vector<Task> tasks;

    static Config load_from_file(const std::string& path,
        const std::string& previous_abs = "");
};
