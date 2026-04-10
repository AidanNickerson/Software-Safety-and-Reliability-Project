// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8

#include "Server.h"
#include "Packet.h"
#include "Packetutils.h"

#include <iostream>
#include <ctime>
#include <fstream>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

bool Server::start(int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    bind(serverSocket, (sockaddr*)&server, sizeof(server));
    listen(serverSocket, 3);

    std::cout << "Server listening...\n";
    logger.open("server_log.txt");
    return true;
}

std::string Server::receive() {
    char buffer[1024] = { 0 };
    int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytes <= 0) return "";
    return std::string(buffer, bytes);
}

void Server::sendMsg(const std::string& msg) {
    send(clientSocket, msg.c_str(), msg.length(), 0);
}

bool Server::handleHandshake() {
    std::string msg = receive();

    if (msg != "HELLO") return false;

    sendMsg("12345");

    std::string response = receive();
    if (response != "RESPONSE|12345_secret") {
        sendMsg("VERIFY_FAIL");
        return false;
    }

    sendMsg("VERIFY_OK");

    currentState = ServerState::Verified;
    std::cout << "State: Verified\n";

    return true;
}

// ---------- DOWNLOAD ----------
void Server::handleDownload() {
    std::ifstream file("telemetry.log", std::ios::binary);

    currentState = ServerState::Transferring;

    file.seekg(0, std::ios::end);
    int size = file.tellg();
    file.seekg(0);

    sendMsg("FILE_INFO|" + std::to_string(size) + "|1024");
    Sleep(100);

    char buffer[1024];
    while (!file.eof()) {
        file.read(buffer, 1024);
        send(clientSocket, buffer, file.gcount(), 0);
    }

    Sleep(50);
    sendMsg("FILE_END");

    currentState = ServerState::Closing;
}

void Server::run() {
    while (true) {
        std::cout << "Waiting...\n";

        sockaddr_in client;
        int c = sizeof(client);
        clientSocket = accept(serverSocket, (sockaddr*)&client, &c);

        currentState = ServerState::Connected;
        std::cout << "State: Connected\n";

        if (!handleHandshake()) continue;

        while (true) {
            std::string req = receive();
            if (req.empty()) break;

            if (req == "REQ_DOWNLOAD") {
                sendMsg("ACK");
                handleDownload();
            }

            if (req == "REQ_SNAPSHOT") {
                sendMsg("ACK");

                FuelPacket p;
                p.header.packetID = 1;
                p.header.type = FUEL_STATUS;
                p.header.aircraftID = 101;
                p.header.timestamp = time(0);

                p.body.fuelLevel = 80.5f;
                p.body.consumptionRate = 2.2f;
                p.body.flightTimeRemaining = 150;
                p.body.nearestAirportID = 5;
                p.body.emergencyFlag = false;
                p.body.alertMessage = (char*)"Normal";

                std::string data = SerializePacket(p);
                sendMsg(data);

                sendMsg("FILE_DONE|" + std::to_string(data.size()));
            }
        }

        closesocket(clientSocket);
        currentState = ServerState::Waiting;
    }
}