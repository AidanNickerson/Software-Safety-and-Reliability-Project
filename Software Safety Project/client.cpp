// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Client.h"
#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <cstring>
#include <windows.h>   // for Sleep()
#include <fstream>     // for file writing

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
// Prepends a binary ProtocolHeader before the payload so every message is
// framed as: [magic|version|msgType|seq|payloadLen] + [payloadLen bytes].
void Client::send(const std::string& payload, MessageType type) {
    ProtocolHeader hdr;
    hdr.magic      = htonl(PROTO_MAGIC);
    hdr.version    = PROTO_VERSION;
    hdr.msgType    = static_cast<uint8_t>(type);
    hdr.seq        = htons(static_cast<uint16_t>(++txSeq));
    hdr.payloadLen = htonl(static_cast<uint32_t>(payload.size()));

    ::send(sock, reinterpret_cast<const char*>(&hdr), sizeof(hdr), 0);
    if (!payload.empty()) {
        ::send(sock, payload.c_str(), static_cast<int>(payload.size()), 0);
    }
}

// ---------- RECEIVE ----------
// Reads the fixed ProtocolHeader first, then dynamically allocates a buffer
// of exactly outHdr.payloadLen bytes and reads the payload into it.
// outHdr.magic == 0 indicates a connection failure.
std::string Client::receive(ProtocolHeader& outHdr) {
    memset(&outHdr, 0, sizeof(outHdr));

    // Read fixed-size header
    char hdrBuf[sizeof(ProtocolHeader)] = { 0 };
    int total = 0;
    while (total < static_cast<int>(sizeof(ProtocolHeader))) {
        int r = recv(sock, hdrBuf + total,
                     static_cast<int>(sizeof(ProtocolHeader)) - total, 0);
        if (r <= 0) {
            std::cout << "Server disconnected or error\n";
            return "";
        }
        total += r;
    }

    memcpy(&outHdr, hdrBuf, sizeof(ProtocolHeader));
    outHdr.magic      = ntohl(outHdr.magic);
    outHdr.seq        = ntohs(outHdr.seq);
    outHdr.payloadLen = ntohl(outHdr.payloadLen);
    ++rxSeq;

    if (outHdr.magic != PROTO_MAGIC) {
        std::cout << "Invalid packet magic\n";
        memset(&outHdr, 0, sizeof(outHdr));
        return "";
    }

    if (outHdr.payloadLen == 0) return "";

    // Dynamically allocate payload buffer based on payloadLen
    char* buf = new char[outHdr.payloadLen + 1];
    total = 0;
    while (total < static_cast<int>(outHdr.payloadLen)) {
        int r = recv(sock, buf + total,
                     static_cast<int>(outHdr.payloadLen) - total, 0);
        if (r <= 0) {
            delete[] buf;
            std::cout << "Server disconnected during payload\n";
            memset(&outHdr, 0, sizeof(outHdr));
            return "";
        }
        total += r;
    }
    buf[outHdr.payloadLen] = '\0';

    std::string result(buf, outHdr.payloadLen);
    delete[] buf;
    return result;
}

// ---------- RUN ----------
void Client::run() {
    txSeq = 0;
    rxSeq = 0;
    state = CONNECTED;

    // ===== STEP 1: SEND HELLO =====
    send("HELLO", MSG_HELLO);
    logger.log("TX", "HELLO", txSeq, 5);

    // Receive challenge
    ProtocolHeader hdr;
    std::string challenge = receive(hdr);
    if (hdr.magic == 0) {
        std::cout << "No challenge received\n";
        return;
    }
    logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

    // ===== STEP 2: SEND RESPONSE =====
    std::string responseMsg = "RESPONSE|" + challenge + "_secret";
    send(responseMsg, MSG_RESPONSE);
    logger.log("TX", "RESPONSE", txSeq, responseMsg.size());

    // ===== STEP 3: VERIFY =====
    receive(hdr);
    logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

    if (hdr.msgType != MSG_VERIFY_OK) {
        std::cout << "Verification failed\n";
        return;
    }

    std::cout << "Verification successful\n";

    // ===== STEP 4: COMMAND MENU =====
    while (state != DISCONNECTED) {
        std::cout << "\n--- Command Menu ---\n";
        std::cout << "[1] Request Snapshot\n";
        std::cout << "[2] Request Download\n";
        std::cout << "[3] Close Session\n";
        std::cout << "Enter command: ";

        std::string input;
        std::getline(std::cin, input);

        if (input == "1" || input == "REQ_SNAPSHOT") {
            // ---- Request Snapshot ----
            // Server generates new telemetry, appends to telemetry.log, and
            // returns the most recent entry.
            send("", MSG_REQ_SNAPSHOT);
            logger.log("TX", "REQ_SNAPSHOT", txSeq, 0);

            std::string ackPayload = receive(hdr);
            if (hdr.magic == 0) break;
            logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

            if (hdr.msgType == MSG_NACK) {
                std::cout << "[ERROR] Server rejected request: " << ackPayload << "\n";
                logger.log("ERROR", "NACK_RECEIVED", rxSeq, hdr.payloadLen);
                continue;
            }

            int bytesReceived = 0;
            std::string data = receive(hdr);
            if (hdr.magic == 0) break;
            logger.log("RX", "DATA", rxSeq, hdr.payloadLen);
            bytesReceived += static_cast<int>(data.size());
            std::cout << "Snapshot: " << data << "\n";

            std::string donePayload = receive(hdr);
            if (hdr.magic == 0) break;
            logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

            if (hdr.msgType == MSG_FILE_DONE) {
                int expectedBytes = std::stoi(donePayload);
                if (bytesReceived == expectedBytes) {
                    std::cout << "Snapshot received: SUCCESS\n";
                    logger.log("INFO", "TRANSFER_OK", 0, bytesReceived);
                } else {
                    std::cout << "ERROR: Byte mismatch! Expected: " << expectedBytes
                              << " Received: " << bytesReceived << "\n";
                    logger.log("ERROR", "BYTE_MISMATCH", 0, bytesReceived);
                }
            }

        } else if (input == "2" || input == "REQ_DOWNLOAD") {
            // ---- Request Download ----
            // Server streams telemetry.log (>= 1 MB) as FILE_CHUNK packets.
            // Client writes each chunk directly to downloaded.log and verifies
            // the total byte count on FILE_DONE.
            send("", MSG_REQ_DOWNLOAD);
            logger.log("TX", "REQ_DOWNLOAD", txSeq, 0);

            // Receive ACK or NACK
            std::string ackPayload = receive(hdr);
            if (hdr.magic == 0) break;
            logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

            if (hdr.msgType == MSG_NACK) {
                std::cout << "[ERROR] Server rejected download: " << ackPayload << "\n";
                logger.log("ERROR", "NACK_RECEIVED", rxSeq, hdr.payloadLen);
                continue;
            }

            // Open output file in binary mode before first chunk arrives
            std::ofstream outFile("downloaded.log", std::ios::binary);
            if (!outFile.is_open()) {
                std::cout << "ERROR: Could not open downloaded.log for writing\n";
                continue;
            }

            int bytesReceived = 0;
            bool transferOk = true;

            // Receive FILE_CHUNK packets until FILE_DONE
            while (true) {
                std::string payload = receive(hdr);
                if (hdr.magic == 0) {
                    // Connection lost mid-transfer
                    outFile.close();
                    transferOk = false;
                    break;
                }
                logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

                if (hdr.msgType == MSG_FILE_CHUNK) {
                    outFile.write(payload.c_str(), static_cast<std::streamsize>(payload.size()));
                    bytesReceived += static_cast<int>(payload.size());

                } else if (hdr.msgType == MSG_FILE_DONE) {
                    outFile.close();
                    int expectedBytes = std::stoi(payload);
                    if (bytesReceived == expectedBytes) {
                        std::cout << "Download complete: SUCCESS ("
                                  << bytesReceived << " bytes saved to downloaded.log)\n";
                        logger.log("INFO", "TRANSFER_OK", 0, bytesReceived);
                    } else {
                        std::cout << "ERROR: Byte mismatch! Expected: " << expectedBytes
                                  << " Received: " << bytesReceived << "\n";
                        logger.log("ERROR", "BYTE_MISMATCH", 0, bytesReceived);
                    }
                    break;

                } else {
                    std::cout << "[ERROR] Unexpected packet during download: "
                              << msgTypeToStr(hdr.msgType) << "\n";
                    outFile.close();
                    transferOk = false;
                    break;
                }
            }

            if (!transferOk) break;

        } else if (input == "3" || input == "REQ_CLOSE") {
            // ---- Graceful Session Close ----
            send("", MSG_REQ_CLOSE);
            logger.log("TX", "REQ_CLOSE", txSeq, 0);

            receive(hdr);
            if (hdr.magic == PROTO_MAGIC) {
                logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);
            }

            logger.log("INFO", "SESSION_END", 0, 0);
            std::cout << "Session closed gracefully\n";
            state = DISCONNECTED;

        } else {
            // ---- Unknown command: send to server to trigger NACK ----
            send(input, MSG_UNKNOWN);
            logger.log("TX", "UNKNOWN", txSeq, input.size());

            std::string nackPayload = receive(hdr);
            if (hdr.magic == 0) break;
            logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

            if (hdr.msgType == MSG_NACK) {
                std::cout << "[ERROR] Invalid command. Server response: " << nackPayload << "\n";
                logger.log("ERROR", "NACK_RECEIVED", rxSeq, hdr.payloadLen);
            }
        }
    }

    std::cout << "Client stopped. State: DISCONNECTED\n";
}
