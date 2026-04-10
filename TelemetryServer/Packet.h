// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once

#include <ctime>
#include <cstdint>
#include <string>

enum PacketType {
    FUEL_STATUS,
    LANDED_SAFE,
    ACK_DIVERT
};

struct PacketHeader {
    int packetID;
    PacketType type;
    int aircraftID;
    time_t timestamp;
};

struct PacketBody {
    float fuelLevel;
    float consumptionRate;
    float flightTimeRemaining;
    int nearestAirportID;
    bool emergencyFlag;
    char* alertMessage;
};

struct FuelPacket {
    PacketHeader header;
    PacketBody body;
};

std::string SerializePacket(const FuelPacket& packet);
FuelPacket DeserializePacket(const std::string& data);

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
