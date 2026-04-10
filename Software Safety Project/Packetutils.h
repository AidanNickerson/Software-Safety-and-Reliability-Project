// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <string>
#include "Packet.h"

std::string SerializePacket(const FuelPacket& packet);
FuelPacket DeserializePacket(const std::string& data);