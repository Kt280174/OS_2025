#pragma once
#include "config.h"

class Worker {
public:
    explicit Worker(const Config& cfg) : cfg_(cfg) {}
    void do_work();

private:
    const Config& cfg_;
    static void clear_folder(const std::string& path);
    static void process_task(const Task& t);
};
