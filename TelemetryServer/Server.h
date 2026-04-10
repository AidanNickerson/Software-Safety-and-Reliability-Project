// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <string>
#include <winsock2.h>
#include "Logger.h"
#include "Packet.h"

// ===== Server State Machine (US-007) =====
enum ServerState {
    STATE_WAITING,      // Listening for a new client connection
    STATE_CONNECTED,    // Client accepted; handshake not yet complete
    STATE_VERIFIED,     // Handshake passed; ready to serve commands
    STATE_TRANSFERRING, // File download in progress
    STATE_CLOSING,      // REQ_CLOSE received; shutting down session
    STATE_ERROR         // Protocol or verification error
};

inline std::string serverStateToStr(ServerState s) {
    switch (s) {
        case STATE_WAITING:      return "WaitingForClient";
        case STATE_CONNECTED:    return "Connected";
        case STATE_VERIFIED:     return "Verified";
        case STATE_TRANSFERRING: return "Transferring";
        case STATE_CLOSING:      return "Closing";
        case STATE_ERROR:        return "Error";
        default:                 return "Unknown";
    }
}

class Server {
public:
    bool start(int port);
    void run();

private:
    SOCKET serverSocket;
    SOCKET clientSocket;

    Logger logger;
    int txSeq = 0;
    int rxSeq = 0;
    ServerState state = STATE_WAITING;

    // Reads fixed ProtocolHeader then exactly hdr.payloadLen bytes.
    // outHdr.magic == 0 on connection failure.
    std::string receive(ProtocolHeader& outHdr);

    // Prepends binary ProtocolHeader before sending payload.
    void sendMsg(const std::string& payload, MessageType type);

    // Reads exactly n bytes into buf; returns false on disconnect/error.
    bool recvExact(char* buf, int n);

    // Transitions to newState, printing and logging the change.
    void setState(ServerState newState);

    bool handleHandshake();

    // Generates a new telemetry entry and appends it to telemetry.log.
    void appendTelemetry();

    // Returns the last non-empty line from telemetry.log (most recent entry).
    std::string readLastTelemetryEntry();

    // Pads telemetry.log with generated entries until it is >= 1 MB.
    void ensureMinFileSize();
};
