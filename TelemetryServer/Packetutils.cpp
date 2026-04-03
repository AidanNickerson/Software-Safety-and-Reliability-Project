// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#include "Packet.h"
#include <sstream>
#include <cstring>

std::string SerializePacket(const FuelPacket& packet) {
    std::stringstream ss;

    ss << packet.header.packetID << "|"
        << packet.header.type << "|"
        << packet.header.aircraftID << "|"
        << packet.header.timestamp << "|"
        << packet.body.fuelLevel << "|"
        << packet.body.consumptionRate << "|"
        << packet.body.flightTimeRemaining << "|"
        << packet.body.nearestAirportID << "|"
        << packet.body.emergencyFlag << "|"
        << packet.body.alertMessage;

    return ss.str();
}

FuelPacket DeserializePacket(const std::string& data) {
    FuelPacket packet;
    std::stringstream ss(data);
    std::string token;

    getline(ss, token, '|'); packet.header.packetID = std::stoi(token);
    getline(ss, token, '|'); packet.header.type = (PacketType)std::stoi(token);
    getline(ss, token, '|'); packet.header.aircraftID = std::stoi(token);
    getline(ss, token, '|'); packet.header.timestamp = std::stol(token);

    getline(ss, token, '|'); packet.body.fuelLevel = std::stof(token);
    getline(ss, token, '|'); packet.body.consumptionRate = std::stof(token);
    getline(ss, token, '|'); packet.body.flightTimeRemaining = std::stof(token);
    getline(ss, token, '|'); packet.body.nearestAirportID = std::stoi(token);
    getline(ss, token, '|'); packet.body.emergencyFlag = std::stoi(token);

    getline(ss, token, '|');

    packet.body.alertMessage = new char[token.length() + 1];
    strcpy_s(packet.body.alertMessage, token.length() + 1, token.c_str());

    return packet;
}