#include "Server.h"
#include "Packet.h"
#include <iostream>
#include <ctime>

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
    std::string msg = receive();
    std::cout << "Received: " << msg << std::endl;

    if (msg != "HELLO") return false;

    std::string challenge = "12345";
    sendMsg(challenge);

    std::string response = receive();
    std::string expected = "RESPONSE|" + challenge + "_secret";

    if (response != expected) {
        sendMsg("VERIFY_FAIL");
        return false;
    }

    sendMsg("VERIFY_OK");
    std::cout << "Client verified\n";
    return true;
}

void Server::run() {
    while (true) {
        std::cout << "Waiting for client...\n";   // DEBUG LINE

        sockaddr_in client;
        int c = sizeof(client);

        clientSocket = accept(serverSocket, (sockaddr*)&client, &c);

        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Accept failed\n";
            continue;
        }

        std::cout << "Client connected!\n";

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

            if (request == "REQ_SNAPSHOT") {
                sendMsg("ACK");

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

                std::cout << "Snapshot sent\n";
            }
        }

        closesocket(clientSocket);
    }
}