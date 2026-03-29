#pragma once

#pragma once
#include <string>

class Client {
public:
    bool connectToServer(const std::string& ip, int port);
    void run();

private:
    int sock;
    std::string receive();
    void send(const std::string& message);
};
