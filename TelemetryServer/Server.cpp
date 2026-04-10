// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#include "Server.h"
#include "Packet.h"
#include "Packetutils.h"   // for SerializePacket

#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>
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

// ---------- STATE MACHINE ----------

// Transitions to newState, prints the change to console, and writes a log entry.
// Invalid transitions are blocked upstream; this function only records valid ones.
void Server::setState(ServerState newState) {
    std::cout << "[STATE] " << serverStateToStr(state)
              << " -> " << serverStateToStr(newState) << "\n";
    logger.log("STATE", serverStateToStr(newState), 0, 0);
    state = newState;
}

// ---------- TELEMETRY HELPERS ----------

void Server::appendTelemetry() {
    static int snapshotCount = 0;
    ++snapshotCount;

    FuelPacket packet;
    packet.header.packetID   = snapshotCount;
    packet.header.type       = FUEL_STATUS;
    packet.header.aircraftID = 101;
    packet.header.timestamp  = time(0);

    packet.body.fuelLevel           = 80.5f - (snapshotCount * 1.5f);
    packet.body.consumptionRate     = 2.2f  + (snapshotCount * 0.1f);
    packet.body.flightTimeRemaining = 150.0f - (snapshotCount * 3.0f);
    packet.body.nearestAirportID    = 5;
    packet.body.emergencyFlag       = (packet.body.fuelLevel < 20.0f);
    packet.body.alertMessage        = packet.body.emergencyFlag
                                        ? (char*)"LOW FUEL"
                                        : (char*)"Normal";

    std::ofstream logFile("telemetry.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << SerializePacket(packet) << "\n";
    }
}

std::string Server::readLastTelemetryEntry() {
    std::ifstream file("telemetry.log");
    if (!file.is_open()) return "";

    std::string line, last;
    while (std::getline(file, line)) {
        if (!line.empty()) last = line;
    }
    return last;
}

void Server::ensureMinFileSize() {
    const size_t MIN_BYTES = 1048576; // 1 MB

    std::ifstream check("telemetry.log", std::ios::binary | std::ios::ate);
    size_t currentSize = check.is_open()
        ? static_cast<size_t>(check.tellg())
        : 0;
    check.close();

    if (currentSize >= MIN_BYTES) return;

    static int padID = 1000000;
    std::ofstream logFile("telemetry.log", std::ios::app);
    if (!logFile.is_open()) return;

    while (currentSize < MIN_BYTES) {
        ++padID;
        FuelPacket p;
        p.header.packetID   = padID;
        p.header.type       = FUEL_STATUS;
        p.header.aircraftID = 101;
        p.header.timestamp  = time(0);

        float fuel = 80.5f - (padID * 0.001f);
        p.body.fuelLevel           = fuel > 0 ? fuel : 0;
        p.body.consumptionRate     = 2.2f;
        p.body.flightTimeRemaining = 150.0f - (padID * 0.002f);
        p.body.nearestAirportID    = 5;
        p.body.emergencyFlag       = (p.body.fuelLevel < 20.0f);
        p.body.alertMessage        = p.body.emergencyFlag
                                       ? (char*)"LOW FUEL"
                                       : (char*)"Normal";

        std::string entry = SerializePacket(p) + "\n";
        logFile << entry;
        currentSize += entry.size();
    }

    std::cout << "telemetry.log padded to " << currentSize << " bytes\n";
}

// ---------- NETWORKING HELPERS ----------

bool Server::recvExact(char* buf, int n) {
    int total = 0;
    while (total < n) {
        int r = recv(clientSocket, buf + total, n - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

std::string Server::receive(ProtocolHeader& outHdr) {
    memset(&outHdr, 0, sizeof(outHdr));

    char hdrBuf[sizeof(ProtocolHeader)] = { 0 };
    if (!recvExact(hdrBuf, sizeof(ProtocolHeader))) {
        return "";
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

    char* buf = new char[outHdr.payloadLen + 1];
    if (!recvExact(buf, static_cast<int>(outHdr.payloadLen))) {
        delete[] buf;
        memset(&outHdr, 0, sizeof(outHdr));
        return "";
    }
    buf[outHdr.payloadLen] = '\0';

    std::string result(buf, outHdr.payloadLen);
    delete[] buf;
    return result;
}

void Server::sendMsg(const std::string& payload, MessageType type) {
    ProtocolHeader hdr;
    hdr.magic      = htonl(PROTO_MAGIC);
    hdr.version    = PROTO_VERSION;
    hdr.msgType    = static_cast<uint8_t>(type);
    hdr.seq        = htons(static_cast<uint16_t>(++txSeq));
    hdr.payloadLen = htonl(static_cast<uint32_t>(payload.size()));

    send(clientSocket, reinterpret_cast<const char*>(&hdr), sizeof(hdr), 0);
    if (!payload.empty()) {
        send(clientSocket, payload.c_str(), static_cast<int>(payload.size()), 0);
    }
}

// ---------- HANDSHAKE ----------

// State on entry: STATE_CONNECTED
// State on success: STATE_VERIFIED
// State on failure: STATE_ERROR
bool Server::handleHandshake() {
    txSeq = 0;
    rxSeq = 0;

    ProtocolHeader hdr;
    std::string msg = receive(hdr);
    std::cout << "Received: " << msgTypeToStr(hdr.msgType) << "\n";
    logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

    // Operational commands in STATE_CONNECTED are invalid transitions
    if (hdr.msgType != MSG_HELLO) {
        std::string nackPayload = "ERR_INVALID_STATE|"
            + serverStateToStr(state) + "|" + msgTypeToStr(hdr.msgType);
        sendMsg(nackPayload, MSG_NACK);
        logger.log("TX", "NACK", txSeq, nackPayload.size());
        logger.log("ERROR", "INVALID_TRANSITION", rxSeq, hdr.payloadLen);
        std::cout << "Blocked: " << msgTypeToStr(hdr.msgType)
                  << " in state " << serverStateToStr(state) << "\n";
        setState(STATE_ERROR);
        return false;
    }

    // Send challenge
    std::string challenge = "12345";
    sendMsg(challenge, MSG_CHALLENGE);
    logger.log("TX", "CHALLENGE", txSeq, challenge.size());

    // Receive response
    std::string response = receive(hdr);
    logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

    std::string expected = "RESPONSE|" + challenge + "_secret";

    if (response != expected || hdr.msgType != MSG_RESPONSE) {
        sendMsg("", MSG_VERIFY_FAIL);
        logger.log("TX", "VERIFY_FAIL", txSeq, 0);
        setState(STATE_ERROR);
        return false;
    }

    sendMsg("", MSG_VERIFY_OK);
    logger.log("TX", "VERIFY_OK", txSeq, 0);

    setState(STATE_VERIFIED);
    std::cout << "Client verified\n";
    return true;
}

// ---------- MAIN LOOP ----------

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

        // Transition: WaitingForClient -> Connected
        setState(STATE_CONNECTED);
        std::cout << "Client connected!\n";

        if (!handleHandshake()) {
            // handleHandshake already set STATE_ERROR; reset for next client
            std::cout << "Handshake failed\n";
            closesocket(clientSocket);
            setState(STATE_WAITING);
            continue;
        }

        // Handle client requests (state is now STATE_VERIFIED)
        while (true) {
            ProtocolHeader hdr;
            std::string request = receive(hdr);

            if (hdr.magic == 0) {
                std::cout << "Client disconnected\n";
                break;
            }

            logger.log("RX", msgTypeToStr(hdr.msgType), rxSeq, hdr.payloadLen);

            // ---- State machine guard: only STATE_VERIFIED accepts commands ----
            if (state != STATE_VERIFIED) {
                std::string nackPayload = "ERR_INVALID_STATE|"
                    + serverStateToStr(state) + "|" + msgTypeToStr(hdr.msgType);
                sendMsg(nackPayload, MSG_NACK);
                logger.log("TX", "NACK", txSeq, nackPayload.size());
                logger.log("ERROR", "INVALID_TRANSITION", rxSeq, hdr.payloadLen);
                std::cout << "Blocked: " << msgTypeToStr(hdr.msgType)
                          << " in state " << serverStateToStr(state) << "\n";
                continue;
            }

            if (hdr.msgType == MSG_REQ_SNAPSHOT) {
                // Verified -> (Verified): snapshot does not change session state
                appendTelemetry();
                std::string entry = readLastTelemetryEntry();

                sendMsg("", MSG_ACK);
                logger.log("TX", "ACK", txSeq, 0);

                sendMsg(entry, MSG_DATA);
                logger.log("TX", "DATA", txSeq, entry.size());

                std::string donePayload = std::to_string(entry.size());
                sendMsg(donePayload, MSG_FILE_DONE);
                logger.log("TX", "FILE_DONE", txSeq, donePayload.size());

                std::cout << "Snapshot sent (" << entry.size() << " bytes)\n";

            } else if (hdr.msgType == MSG_REQ_DOWNLOAD) {
                // Transition: Verified -> Transferring -> Verified
                setState(STATE_TRANSFERRING);

                ensureMinFileSize();

                std::ifstream file("telemetry.log", std::ios::binary | std::ios::ate);
                if (!file.is_open()) {
                    std::string nackPayload = "ERR_FILE_NOT_FOUND|telemetry.log";
                    sendMsg(nackPayload, MSG_NACK);
                    logger.log("TX", "NACK", txSeq, nackPayload.size());
                    std::cout << "Download requested but telemetry.log is missing\n";
                    setState(STATE_VERIFIED); // recover
                    continue;
                }
                file.seekg(0, std::ios::beg);

                sendMsg("", MSG_ACK);
                logger.log("TX", "ACK", txSeq, 0);

                const size_t CHUNK_SIZE = 8192;
                char chunkBuf[CHUNK_SIZE];
                size_t bytesSent = 0;

                while (file.read(chunkBuf, CHUNK_SIZE) || file.gcount() > 0) {
                    size_t bytesRead = static_cast<size_t>(file.gcount());
                    sendMsg(std::string(chunkBuf, bytesRead), MSG_FILE_CHUNK);
                    logger.log("TX", "FILE_CHUNK", txSeq, bytesRead);
                    bytesSent += bytesRead;
                }
                file.close();

                sendMsg(std::to_string(bytesSent), MSG_FILE_DONE);
                logger.log("TX", "FILE_DONE", txSeq, std::to_string(bytesSent).size());

                std::cout << "Download complete: " << bytesSent << " bytes in "
                          << ((bytesSent + CHUNK_SIZE - 1) / CHUNK_SIZE) << " chunks\n";

                // Transition: Transferring -> Verified
                setState(STATE_VERIFIED);

            } else if (hdr.msgType == MSG_REQ_CLOSE) {
                // Transition: Verified -> Closing -> WaitingForClient
                setState(STATE_CLOSING);

                sendMsg("", MSG_ACK);
                logger.log("TX", "ACK", txSeq, 0);
                logger.log("INFO", "SESSION_END", 0, 0);
                std::cout << "Session closed gracefully by client\n";
                break; // setState(STATE_WAITING) happens after closesocket below

            } else {
                // Unknown command in STATE_VERIFIED: NACK, stay in Verified
                std::string cmdStr = request.empty()
                    ? msgTypeToStr(hdr.msgType)
                    : request;
                std::string nackPayload = "ERR_UNKNOWN_CMD|" + cmdStr;
                sendMsg(nackPayload, MSG_NACK);
                logger.log("TX", "NACK", txSeq, nackPayload.size());
                logger.log("ERROR", "INVALID_CMD", rxSeq, request.size());
                std::cout << "Unknown command from client: " << cmdStr << "\n";
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
        setState(STATE_WAITING);
    }
}
