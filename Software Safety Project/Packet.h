// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once

#include <ctime>
#include <cstdint>
#include <string>

// Packet types
enum PacketType {
    FUEL_STATUS,   // Regular fuel status report
    LANDED_SAFE,   // Aircraft has landed safely
    ACK_DIVERT     // Acknowledgement of emergency divert command
};

// Header
struct PacketHeader {
    int packetID;      // Unique packet identifier
    PacketType type;   // Type of packet being transmitted
    int aircraftID;    // REQ-PKT-010: Unique aircraft identifier
    time_t timestamp;  // Time of packet creation on client side
};

// Body
struct PacketBody {
    float fuelLevel;           // REQ-PKT-020: Current fuel level (0-100%)
    float consumptionRate;     // REQ-PKT-030: Fuel consumption rate per minute
    float flightTimeRemaining; // REQ-PKT-040: Estimated flight time remaining
    int nearestAirportID;      // REQ-PKT-050: ID of nearest available airport
    bool emergencyFlag;        // REQ-PKT-060: True if aircraft is in emergency fuel state
    char* alertMessage;        // REQ-PKT-070: Dynamically allocated alert/status message
};

// Full packet
struct FuelPacket {
    PacketHeader header; // Packet metadata
    PacketBody body;     // Fuel status payload
};

// ===== Protocol Framing (User Story: Fixed Header + Variable Payload) =====

const uint32_t PROTO_MAGIC   = 0xDEAD1234;
const uint8_t  PROTO_VERSION = 1;

enum MessageType : uint8_t {
    MSG_HELLO        = 0,
    MSG_CHALLENGE    = 1,
    MSG_RESPONSE     = 2,
    MSG_VERIFY_OK    = 3,
    MSG_VERIFY_FAIL  = 4,
    MSG_NACK         = 5,
    MSG_REQ_SNAPSHOT = 6,
    MSG_ACK          = 7,
    MSG_DATA         = 8,
    MSG_FILE_DONE    = 9,
    MSG_REQ_CLOSE    = 10,
    MSG_REQ_DOWNLOAD = 11,
    MSG_FILE_CHUNK   = 12,
    MSG_UNKNOWN      = 0xFF
};

// Fixed header sent before every payload; receiver reads this first, then
// allocates exactly payloadLen bytes for the payload.
#pragma pack(push, 1)
struct ProtocolHeader {
    uint32_t magic;      // Must equal PROTO_MAGIC
    uint8_t  version;    // Protocol version
    uint8_t  msgType;    // MessageType enum value
    uint16_t seq;        // Sender sequence number
    uint32_t payloadLen; // Number of payload bytes that follow
};
#pragma pack(pop)

inline std::string msgTypeToStr(uint8_t t) {
    switch (static_cast<MessageType>(t)) {
        case MSG_HELLO:        return "HELLO";
        case MSG_CHALLENGE:    return "CHALLENGE";
        case MSG_RESPONSE:     return "RESPONSE";
        case MSG_VERIFY_OK:    return "VERIFY_OK";
        case MSG_VERIFY_FAIL:  return "VERIFY_FAIL";
        case MSG_NACK:         return "NACK";
        case MSG_REQ_SNAPSHOT: return "REQ_SNAPSHOT";
        case MSG_ACK:          return "ACK";
        case MSG_DATA:         return "DATA";
        case MSG_FILE_DONE:    return "FILE_DONE";
        case MSG_REQ_CLOSE:    return "REQ_CLOSE";
        case MSG_REQ_DOWNLOAD: return "REQ_DOWNLOAD";
        case MSG_FILE_CHUNK:   return "FILE_CHUNK";
        default:               return "UNKNOWN";
    }
}
