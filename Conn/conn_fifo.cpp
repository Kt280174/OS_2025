#include "conn.h"
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

class FifoConnection : public Connection {
private:
    static constexpr const char* FIFO_PREFIX = "/tmp/myfifo";
    std::string fifo_name_;

public:
    FifoConnection(size_t id, bool create) {
        fifo_name_ = std::string(FIFO_PREFIX) + std::to_string(id);
        is_creator_ = create;

        if (create && mkfifo(fifo_name_.c_str(), 0666) < 0 && errno != EEXIST) {
            std::cerr << "Error creating FIFO: " << fifo_name_ << std::endl;
        }

        descriptor_ = open(fifo_name_.c_str(), O_RDWR);
        if (descriptor_ == -1) {
            std::cerr << "Error opening FIFO: " << fifo_name_ << std::endl;
        }
    }

    ~FifoConnection() override {
        if (descriptor_ != -1) {
            close(descriptor_);
        }
        if (is_creator_) {
            unlink(fifo_name_.c_str());
        }
    }

    bool Read(void* buffer, size_t count) override {
        ssize_t bytes_read = read(descriptor_, buffer, count);
        return bytes_read == static_cast<ssize_t>(count);
    }

    bool Write(void* buffer, size_t count) override {
        ssize_t bytes_written = write(descriptor_, buffer, count);
        return bytes_written == static_cast<ssize_t>(count);
    }
};

Connection* Connection::create(size_t id, bool create, size_t msg_size) {
    return new FifoConnection(id, create);
}

Connection::~Connection() = default;