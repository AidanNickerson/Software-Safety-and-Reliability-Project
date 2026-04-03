// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <string>
#include <winsock2.h>
#include "Logger.h"

class Server {
public:
    bool start(int port);
    void run();

private:
    SOCKET serverSocket;
    SOCKET clientSocket;

    Logger logger;
    int txSeq = 0;
    int rxSeq = 0;
    bool verified = false;

    std::string receive();
    void sendMsg(const std::string& message);

    bool handleHandshake();
};