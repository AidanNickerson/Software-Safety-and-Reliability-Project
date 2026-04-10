// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#include "Server.h"
#include "Packet.h"
#include "Packetutils.h"   // for SerializePacket

#include <iostream>
#include <ctime>
#include <fstream>   // for file handling
#include <windows.h> // for Sleep to separate packets

#pragma comment(lib, "ws2_32.lib")

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

    logger.open("server_log.txt");

    return true;
}

std::string Server::receive() {
    char buffer[1024] = { 0 };
    int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytes <= 0) {
        return "";
    }

    return std::string(buffer, bytes);
}

void Server::sendMsg(const std::string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

bool Server::handleHandshake() {
    verified = false;
    txSeq = 0;
    rxSeq = 0;

    std::string msg = receive();
    std::cout << "Received: " << msg << std::endl;
    logger.log("RX", getMsgType(msg), ++rxSeq, msg.size());

    // Reject operational commands received before verification is complete
    if (msg == "REQ_SNAPSHOT" || msg == "REQ_DOWNLOAD") {
        std::cout << "ERROR: Rejected unverified request: " << msg << "\n";
        std::string nack = "NACK|NOT_VERIFIED";
        sendMsg(nack);
        logger.log("TX", "NACK", ++txSeq, nack.size());
        return false;
    }

    if (msg != "HELLO") return false;

    std::string challenge = "12345";
    sendMsg(challenge);
    logger.log("TX", "CHALLENGE", ++txSeq, challenge.size());

    std::string response = receive();
    logger.log("RX", getMsgType(response), ++rxSeq, response.size());

    std::string expected = "RESPONSE|" + challenge + "_secret";

    if (response != expected) {
        std::string fail = "VERIFY_FAIL";
        sendMsg(fail);
        logger.log("TX", "VERIFY_FAIL", ++txSeq, fail.size());
        return false;
    }

    std::string ok = "VERIFY_OK";
    sendMsg(ok);
    logger.log("TX", "VERIFY_OK", ++txSeq, ok.size());

    verified = true;

    // update state after verification
    currentState = ServerState::Verified;
    std::cout << "State: Verified\n";

    std::cout << "Client verified\n";
    return true;
}

// ---------- DOWNLOAD HANDLER ----------
void Server::handleDownload() {
    std::ifstream file("telemetry.log", std::ios::binary);

    // new: state update
    currentState = ServerState::Transferring;
    std::cout << "State: Transferring\n";

    // check if file exists
    if (!file) {
        std::string err = "ERROR|File not found";
        sendMsg(err);
        logger.log("TX", "ERROR", ++txSeq, err.size());
        return;
    }

    // get file size
    file.seekg(0, std::ios::end);
    int totalSize = file.tellg();
    file.seekg(0, std::ios::beg);

    int chunkSize = 1024;

    // send FILE_INFO (metadata)
    std::string info = "FILE_INFO|" + std::to_string(totalSize) + "|" + std::to_string(chunkSize);
    sendMsg(info);
    logger.log("TX", "FILE_INFO", ++txSeq, info.size());

    Sleep(100);

    char buffer[1024];

    while (true) {
        file.read(buffer, chunkSize);
        int bytesRead = file.gcount();

        if (bytesRead <= 0) break;

        send(clientSocket, buffer, bytesRead, 0);
        Sleep(10);
    }

    Sleep(50);

    std::string endMsg = "FILE_END";
    sendMsg(endMsg);
    logger.log("TX", "FILE_END", ++txSeq, endMsg.size());

    file.close();

    // state update after transfer
    currentState = ServerState::Closing;
    std::cout << "State: Closing\n";

    std::cout << "File sent successfully\n";
}

void Server::run() {
    while (true) {
        std::cout << "Waiting for client...\n";

        sockaddr_in client;
        int c = sizeof(client);

        clientSocket = accept(serverSocket, (sockaddr*)&client, &c);

        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Accept failed\n";
            continue;
        }

        std::cout << "Client connected!\n";

        // update state
        currentState = ServerState::Connected;
        std::cout << "State: Connected\n";

        if (!handleHandshake()) {
            std::cout << "Handshake failed\n";
            closesocket(clientSocket);
            continue;
        }

        while (true) {
            std::string request = receive();

            if (request.empty()) {
                std::cout << "Client disconnected\n";
                break;
            }

            logger.log("RX", getMsgType(request), ++rxSeq, request.size());

            if (request == "REQ_SNAPSHOT" || request == "REQ_DOWNLOAD") {
                if (!verified) {
                    std::cout << "ERROR: Rejected unverified request: " << request << "\n";
                    std::string nack = "NACK|NOT_VERIFIED";
                    sendMsg(nack);
                    logger.log("TX", "NACK", ++txSeq, nack.size());
                    continue;
                }
            }

            if (request == "REQ_SNAPSHOT") {
                std::string ack = "ACK";
                sendMsg(ack);
                logger.log("TX", "ACK", ++txSeq, ack.size());

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

                std::string data = SerializePacket(packet);
                sendMsg(data);
                logger.log("TX", "DATA", ++txSeq, data.size());

                std::cout << "Snapshot sent\n";
            }

            // handle file download request
            if (request == "REQ_DOWNLOAD") {

                // new: enforce valid state
                if (currentState != ServerState::Verified) {
                    std::string nack = "NACK|INVALID_STATE";
                    sendMsg(nack);
                    logger.log("TX", "NACK", ++txSeq, nack.size());
                    continue;
                }

                std::string ack = "ACK";
                sendMsg(ack);
                logger.log("TX", "ACK", ++txSeq, ack.size());

                handleDownload();
            }
        }

        closesocket(clientSocket);

        // reset state
        currentState = ServerState::Waiting;
        std::cout << "State: Waiting\n";
    }
}