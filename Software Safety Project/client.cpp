// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Client.h"
#include <iostream>
#include <winsock2.h>
#include <windows.h>   // for Sleep()

#pragma comment(lib, "ws2_32.lib")

// Establish connection to server
bool Client::connectToServer(const std::string& ip, int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // Attempt to connect
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        std::cout << "Connection failed\n";
        return false;
    }

    std::cout << "CONNECTED\n";

    // Open log file
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

    // If connection closed or error
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

    int bytesReceived = 0;  // Track total bytes received for validation

    // ===== STEP 1: SEND HELLO =====
    {
        std::string hello = "HELLO";
        send(hello);
        logger.log("TX", "HELLO", ++txSeq, hello.size());
    }

    // Receive challenge from server
    std::string challenge = receive();

    if (challenge.empty()) {
        std::cout << "No challenge received\n";
        return;
    }
    logger.log("RX", getMsgType(challenge), ++rxSeq, challenge.size());

    // ===== STEP 2: SEND RESPONSE =====
    std::string response = challenge + "_secret";
    std::string responseMsg = "RESPONSE|" + response;
    send(responseMsg);
    logger.log("TX", "RESPONSE", ++txSeq, responseMsg.size());

    // ===== STEP 3: VERIFY =====
    std::string verify = receive();
    logger.log("RX", getMsgType(verify), ++rxSeq, verify.size());

    if (verify != "VERIFY_OK") {
        std::cout << "Verification failed\n";
        return;
    }

    std::cout << "Verification successful\n";

    // ===== STEP 4: CONTINUOUS SNAPSHOT REQUEST =====
    while (true) {
        // Send snapshot request
        std::string req = "REQ_SNAPSHOT";
        send(req);
        logger.log("TX", "REQ_SNAPSHOT", ++txSeq, req.size());

        // Receive ACK or NACK
        std::string ack = receive();
        if (ack.empty()) {
            break;
        }
        logger.log("RX", getMsgType(ack), ++rxSeq, ack.size());

        if (ack.find("NACK") != std::string::npos) {
            std::cout << "Server rejected request\n";
            break;
        }

        // Receive telemetry data
        std::string data = receive();
        if (data.empty()) {
            break;
        }
        logger.log("RX", "DATA", ++rxSeq, data.size());

        // Add to total bytes received
        bytesReceived += data.size();

        std::cout << "Snapshot: " << data << std::endl;

        // ===== RECEIVE FILE_DONE MESSAGE =====
        std::string doneMsg = receive();
        if (doneMsg.empty()) {
            break;
        }

        logger.log("RX", getMsgType(doneMsg), ++rxSeq, doneMsg.size());

        // Validate total bytes against server value
        if (doneMsg.find("FILE_DONE") != std::string::npos) {
            size_t pos = doneMsg.find("|");

            if (pos != std::string::npos) {
                int expectedBytes = std::stoi(doneMsg.substr(pos + 1));

                // Compare expected vs actual bytes
                if (bytesReceived == expectedBytes) {
                    std::cout << "File transfer complete: SUCCESS\n";
                    logger.log("INFO", "TRANSFER_OK", 0, bytesReceived);
                }
                else {
                    std::cout << "ERROR: Byte mismatch!\n";
                    std::cout << "Expected: " << expectedBytes
                        << " Received: " << bytesReceived << std::endl;

                    logger.log("ERROR", "BYTE_MISMATCH", 0, bytesReceived);
                }
            }

            break; // End transfer after FILE_DONE
        }

        // Wait before next request
        Sleep(2000);
    }

    std::cout << "Client stopped\n";
}