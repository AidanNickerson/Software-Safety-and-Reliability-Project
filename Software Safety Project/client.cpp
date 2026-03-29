#include "Client.h"
#include <iostream>
#include <winsock2.h>

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
    return true;
}

void Client::send(const std::string& message) {
    ::send(sock, message.c_str(), message.length(), 0);
}

std::string Client::receive() {
    char buffer[1024];
    int bytes = recv(sock, buffer, sizeof(buffer), 0);
    return std::string(buffer, bytes);
}

void Client::run() {
    send("HELLO");
    std::string challenge = receive();

    std::string response = challenge + "_secret";
    send("RESPONSE|" + response);

    std::string verify = receive();
    if (verify != "VERIFY_OK") {
        std::cout << "Verification failed\n";
        return;
    }

    send("REQ_SNAPSHOT");

    std::string ack = receive();
    if (ack.find("NACK") != std::string::npos) {
        std::cout << ack << std::endl;
        return;
    }

    std::string data = receive();
    std::cout << "Snapshot: " << data << std::endl;
}