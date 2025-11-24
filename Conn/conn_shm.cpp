#include "conn.h"
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

class ShmConnection : public Connection {
private:
    static constexpr const char* SHM_PREFIX = "myshm";
    static constexpr size_t BUFFER_SIZE = 1024;

    std::string shm_name_;
    void* shared_memory_{ nullptr };

public:
    ShmConnection(size_t id, bool create) {
        shm_name_ = std::string(SHM_PREFIX) + std::to_string(id);
        is_creator_ = create;

        int flags = create ? O_CREAT | O_RDWR : O_RDWR;
        descriptor_ = shm_open(shm_name_.c_str(), flags, 0666);

        if (descriptor_ == -1) {
            std::cerr << "Error opening shared memory: " << shm_name_ << std::endl;
            return;
        }

        if (create && ftruncate(descriptor_, BUFFER_SIZE) == -1) {
            std::cerr << "Error setting shared memory size" << std::endl;
            close(descriptor_);
            return;
        }

        shared_memory_ = mmap(nullptr, BUFFER_SIZE, PROT_READ | PROT_WRITE,
            MAP_SHARED, descriptor_, 0);
        if (shared_memory_ == MAP_FAILED) {
            std::cerr << "Error mapping shared memory" << std::endl;
            close(descriptor_);
            if (create) {
                shm_unlink(shm_name_.c_str());
            }
        }
    }

    ~ShmConnection() override {
        if (shared_memory_ != MAP_FAILED) {
            munmap(shared_memory_, BUFFER_SIZE);
        }
        if (descriptor_ != -1) {
            close(descriptor_);
        }
        if (is_creator_) {
            shm_unlink(shm_name_.c_str());
        }
    }

    bool Read(void* buffer, size_t count) override {
        if (count > BUFFER_SIZE) return false;
        std::memcpy(buffer, shared_memory_, count);
        return true;
    }

    bool Write(void* buffer, size_t count) override {
        if (count > BUFFER_SIZE) return false;
        std::memcpy(shared_memory_, buffer, count);
        return true;
    }
};

Connection* Connection::create(size_t id, bool create, size_t msg_size) {
    return new ShmConnection(id, create);
}

Connection::~Connection() = default;