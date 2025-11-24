#pragma once

#include <cstddef>

class Connection {
public:
    static Connection* create(size_t id, bool create, size_t msg_size = 0);

    virtual ~Connection() = 0;

    virtual bool Read(void* buffer, size_t count) = 0;
    virtual bool Write(void* buffer, size_t count) = 0;

protected:
    int descriptor_{ -1 };
    bool is_creator_{ false };
};