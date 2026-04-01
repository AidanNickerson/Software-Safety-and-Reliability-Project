#pragma once
#include <string>
#include <winsock2.h>

class Server {
public:
    bool start(int port);
    void run();

private:
    SOCKET serverSocket;
    SOCKET clientSocket;

    std::string receive();
    void sendMsg(const std::string& message);

    bool handleHandshake();
};