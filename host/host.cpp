#include "wolf_server.h"
#include <iostream>
#include <unistd.h>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <max_goats>" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        size_t max_goats = std::stoul(argv[1]);

        if (max_goats == 0) {
            std::cerr << "Error: Maximum goats must be positive" << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Wolf Server PID: " << getpid() << std::endl;
        std::cout << "Maximum goats: " << max_goats << std::endl;

        Wolf& wolf = Wolf::instance();
        wolf.set_max_goats_num(max_goats);

        return wolf.run();

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}