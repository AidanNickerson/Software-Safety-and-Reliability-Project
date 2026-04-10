// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <string>
#include <winsock2.h>
#include "Logger.h"
#include "Packet.h"

enum ClientState { CONNECTED, DISCONNECTED };

class Client {
public:
    bool connectToServer(const std::string& ip, int port);
    void run();

private:
    SOCKET sock;
    Logger logger;
    int txSeq = 0;
    int rxSeq = 0;
    ClientState state = DISCONNECTED;

    // Reads fixed ProtocolHeader then exactly hdr.payloadLen bytes.
    // outHdr.magic == 0 on connection failure.
    std::string receive(ProtocolHeader& outHdr);

    // Prepends binary ProtocolHeader before sending payload.
    void send(const std::string& payload, MessageType type);
};
