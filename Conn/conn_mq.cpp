#include "conn.h"
#include <iostream>
#include <mqueue.h>
#include <syslog.h>

class MqConnection : public Connection {
private:
    static constexpr const char* MQ_PREFIX = "/mq";
    std::string mq_name_;

public:
    MqConnection(size_t id, bool create, size_t msg_size) {
        mq_name_ = std::string(MQ_PREFIX) + std::to_string(id);
        is_creator_ = create;

        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT;

            struct mq_attr attributes {};
            attributes.mq_flags = 0;
            attributes.mq_maxmsg = 10;
            attributes.mq_msgsize = msg_size;
            attributes.mq_curmsgs = 0;

            descriptor_ = mq_open(mq_name_.c_str(), flags, 0666, &attributes);
        }
        else {
            descriptor_ = mq_open(mq_name_.c_str(), flags);
        }

        if (descriptor_ == -1) {
            syslog(LOG_ERR, "Failed to open message queue: %s", mq_name_.c_str());
        }
    }

    ~MqConnection() override {
        if (descriptor_ != -1) {
            mq_close(descriptor_);
        }
        if (is_creator_) {
            mq_unlink(mq_name_.c_str());
        }
    }

    bool Read(void* buffer, size_t count) override {
        return mq_receive(descriptor_, static_cast<char*>(buffer), count, nullptr) != -1;
    }

    bool Write(void* buffer, size_t count) override {
        return mq_send(descriptor_, static_cast<char*>(buffer), count, 0) != -1;
    }
};

Connection* Connection::create(size_t id, bool create, size_t msg_size) {
    return new MqConnection(id, create, msg_size);
}

Connection::~Connection() = default;