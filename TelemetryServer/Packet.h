#pragma once

#include <ctime>
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