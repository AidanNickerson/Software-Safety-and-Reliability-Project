// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <string>
#include "Logger.h"

class Client {
public:
    bool connectToServer(const std::string& ip, int port);
    void run();

private:
    int sock;
    Logger logger;
    int txSeq = 0;
    int rxSeq = 0;

    std::string receive();
    void send(const std::string& message);

    // new function to handle file download
    void downloadFile();
};