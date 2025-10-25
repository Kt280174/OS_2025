#include "daemon.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::string conf = (argc > 1) ? argv[1] : "config.txt";
    return Daemon::instance().run(conf);
}
