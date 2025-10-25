#include "config.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

static inline std::string trim(std::string s) {
    auto ns = [](int ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
    s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
    return s;
}

Config Config::load_from_file(const std::string& path, const std::string& prev_abs) {
    std::ifstream in(path);
    if (!in.is_open()) throw std::runtime_error("Cannot open config: " + path);

    Config cfg;
    cfg.abs_path = prev_abs.empty() ? to_abs_path(path) : prev_abs;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // key=value ?
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string k = trim(line.substr(0, pos));
            std::string v = trim(line.substr(pos + 1));
            if (k == "interval" && !v.empty()) {
                try { cfg.interval = std::max(1, std::stoi(v)); }
                catch (...) {}
            }
            else if (k == "pid_file" && !v.empty()) {
                cfg.pid_file = v;
            }
            continue;
        }

        // otherwise: task line: folder1 folder2 ext subfolder
        std::istringstream iss(line);
        Task t;
        if (iss >> t.src >> t.dst >> t.ext >> t.subfolder) {
            cfg.tasks.push_back(std::move(t));
        }
    }

    if (cfg.tasks.empty()) throw std::runtime_error("Config has no tasks");
    return cfg;
}
