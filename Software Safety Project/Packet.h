// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once

#include <ctime>

// Packet types
enum PacketType {
    FUEL_STATUS = 0,   // Regular fuel status report
    LANDED_SAFE = 1,   // Aircraft has landed safely
    ACK_DIVERT = 2,    // Acknowledgement of emergency divert command

    // new packet types for file download feature
    REQ_DOWNLOAD = 10,   // client request
    FILE_INFO = 11,      // server metadata
    FILE_CHUNK = 12,     // (optional, not used now)
    FILE_END = 13,       // transfer complete
    ERROR_MSG = 14       // error case
};

// Header
struct PacketHeader {
    int packetID;          // Unique packet identifier
    PacketType type;       // Type of packet being transmitted
    int aircraftID;        // REQ-PKT-010: Unique aircraft identifier
    time_t timestamp;     // Time of packet creation on client side
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