// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8

#include "Server.h"
#include "Packet.h"
#include <iostream>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

// Initialize Winsock, create socket, bind, and start listening
bool Server::start(int port) {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "Bind failed\n";
        return false;
    }

    listen(serverSocket, 3);

    std::cout << "Server listening on port " << port << "...\n";

    // Open log file
    logger.open("server_log.txt");

    return true;
}

// Receive message from client
std::string Server::receive() {
    char buffer[1024] = { 0 };
    int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(buffer, bytes);
}

// Send message to client
void Server::sendMsg(const std::string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

// Handles authentication handshake before allowing operations
bool Server::handleHandshake() {
    verified = false;
    txSeq = 0;
    rxSeq = 0;

    std::string msg = receive();
    std::cout << "Received: " << msg << std::endl;
    logger.log("RX", getMsgType(msg), ++rxSeq, msg.size());

    // Reject operational commands before verification
    if (msg == "REQ_SNAPSHOT" || msg == "REQ_DOWNLOAD") {
        std::cout << "ERROR: Rejected unverified request: " << msg << "\n";
        std::string nack = "NACK|NOT_VERIFIED";
        sendMsg(nack);
        logger.log("TX", "NACK", ++txSeq, nack.size());
        return false;
    }

    // Expect HELLO to begin handshake
    if (msg != "HELLO") return false;

    // Send challenge value
    std::string challenge = "12345";
    sendMsg(challenge);
    logger.log("TX", "CHALLENGE", ++txSeq, challenge.size());

    // Receive response
    std::string response = receive();
    logger.log("RX", getMsgType(response), ++rxSeq, response.size());

    // Validate response
    std::string expected = "RESPONSE|" + challenge + "_secret";

    if (response != expected) {
        std::string fail = "VERIFY_FAIL";
        sendMsg(fail);
        logger.log("TX", "VERIFY_FAIL", ++txSeq, fail.size());
        return false;
    }

    // Send verification success
    std::string ok = "VERIFY_OK";
    sendMsg(ok);
    logger.log("TX", "VERIFY_OK", ++txSeq, ok.size());

    verified = true;
    std::cout << "Client verified\n";
    return true;
}

// Main server loop
void Server::run() {
    while (true) {
        std::cout << "Waiting for client...\n";

        sockaddr_in client;
        int c = sizeof(client);

        // Accept incoming connection
        clientSocket = accept(serverSocket, (sockaddr*)&client, &c);

        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Accept failed\n";
            continue;
        }

        std::cout << "Client connected!\n";

        // Perform handshake
        if (!handleHandshake()) {
            std::cout << "Handshake failed\n";
            closesocket(clientSocket);
            continue;
        }

        // Handle client requests
        while (true) {
            std::string request = receive();

            if (request.empty()) {
                std::cout << "Client disconnected\n";
                break;
            }

            logger.log("RX", getMsgType(request), ++rxSeq, request.size());

            // Reject commands if somehow not verified
            if (request == "REQ_SNAPSHOT" || request == "REQ_DOWNLOAD") {
                if (!verified) {
                    std::cout << "ERROR: Rejected unverified request: " << request << "\n";
                    std::string nack = "NACK|NOT_VERIFIED";
                    sendMsg(nack);
                    logger.log("TX", "NACK", ++txSeq, nack.size());
                    continue;
                }
            }

            // Handle snapshot request
            if (request == "REQ_SNAPSHOT") {
                // Send ACK
                std::string ack = "ACK";
                sendMsg(ack);
                logger.log("TX", "ACK", ++txSeq, ack.size());

                // Create telemetry packet
                FuelPacket packet;

                packet.header.packetID = 1;
                packet.header.type = FUEL_STATUS;
                packet.header.aircraftID = 101;
                packet.header.timestamp = time(0);

                packet.body.fuelLevel = 80.5f;
                packet.body.consumptionRate = 2.2f;
                packet.body.flightTimeRemaining = 150.0f;
                packet.body.nearestAirportID = 5;
                packet.body.emergencyFlag = false;
                packet.body.alertMessage = (char*)"Normal";

                // Serialize packet into string format
                std::string data = SerializePacket(packet);

                // Send packet data
                sendMsg(data);
                logger.log("TX", "DATA", ++txSeq, data.size());

                // ===== FILE TRANSFER COMPLETION SIGNAL =====
                // Send total bytes so client can verify integrity
                int totalBytes = data.size();
                std::string doneMsg = "FILE_DONE|" + std::to_string(totalBytes);
                sendMsg(doneMsg);
                logger.log("TX", "FILE_DONE", ++txSeq, doneMsg.size());

                std::cout << "Snapshot sent\n";
            }
        }

        closesocket(clientSocket);
    }
}