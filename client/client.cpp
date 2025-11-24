#include "goat_client.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <host_pid>" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        pid_t host_pid = std::stoi(argv[1]);

        if (host_pid <= 0) {
            std::cerr << "Error: Invalid host PID" << std::endl;
            return EXIT_FAILURE;
        }

        Goat& goat = Goat::instance();
        if (!goat.connect_to_host(host_pid)) {
            std::cerr << "Error: Failed to connect to host" << std::endl;
            return EXIT_FAILURE;
        }

        return goat.run();

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}