// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Client.h"
#include <iostream>
#include <winsock2.h>
#include <windows.h>   // for Sleep()

#pragma comment(lib, "ws2_32.lib")

bool Client::connectToServer(const std::string& ip, int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        std::cout << "Connection failed\n";
        return false;
    }

    std::cout << "CONNECTED\n";

    logger.open("client_log.txt");

    return true;
}

// ---------- SEND ----------
void Client::send(const std::string& message) {
    ::send(sock, message.c_str(), message.length(), 0);
}

// ---------- RECEIVE ----------
std::string Client::receive() {
    char buffer[1024] = { 0 };
    int bytes = recv(sock, buffer, sizeof(buffer), 0);

    if (bytes <= 0) {
        std::cout << "Server disconnected or error\n";
        return "";
    }

    return std::string(buffer, bytes);
}

// ---------- RUN ----------
void Client::run() {
    txSeq = 0;
    rxSeq = 0;

    // Step 1: HELLO
    {
        std::string hello = "HELLO";
        send(hello);
        logger.log("TX", "HELLO", ++txSeq, hello.size());
    }
    std::string challenge = receive();

    if (challenge.empty()) {
        std::cout << "No challenge received\n";
        return;
    }
    logger.log("RX", getMsgType(challenge), ++rxSeq, challenge.size());

    // Step 2: RESPONSE
    std::string response = challenge + "_secret";
    std::string responseMsg = "RESPONSE|" + response;
    send(responseMsg);
    logger.log("TX", "RESPONSE", ++txSeq, responseMsg.size());

    // Step 3: VERIFY
    std::string verify = receive();
    logger.log("RX", getMsgType(verify), ++rxSeq, verify.size());

    if (verify != "VERIFY_OK") {
        std::cout << "Verification failed\n";
        return;
    }

    std::cout << "Verification successful\n";

    // Step 4: CONTINUOUS SNAPSHOT REQUEST
    while (true) {
        std::string req = "REQ_SNAPSHOT";
        send(req);
        logger.log("TX", "REQ_SNAPSHOT", ++txSeq, req.size());

        std::string ack = receive();
        if (ack.empty()) {
            break;
        }
        logger.log("RX", getMsgType(ack), ++rxSeq, ack.size());

        if (ack.find("NACK") != std::string::npos) {
            std::cout << "Server rejected request\n";
            break;
        }

        std::string data = receive();
        if (data.empty()) {
            break;
        }
        logger.log("RX", "DATA", ++rxSeq, data.size());

        std::cout << "Snapshot: " << data << std::endl;

        // wait 2 seconds before next request
        Sleep(2000);
    }

    std::cout << "Client stopped\n";
}