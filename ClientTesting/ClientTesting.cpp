#include "pch.h"
#include "CppUnitTest.h"

// Pull in the client-side headers and implementation
#include "../Software Safety Project/Packet.h"
#include "../Software Safety Project/Packetutils.h"
#include "../Software Safety Project/Logger.h"

// Include the implementation directly so the linker finds the definitions
// without needing to add the source project as a dependency.
#include "../Software Safety Project/Pakcetutils.cpp"

#include <string>
#include <fstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// ---------------------------------------------------------------------------
// Helper: build a fully populated FuelPacket for reuse across tests
// ---------------------------------------------------------------------------
static FuelPacket MakeClientTestPacket(int id = 1, int aircraftID = 101,
                                       float fuel = 75.0f, bool emergency = false,
                                       const char* msg = "Normal")
{
    FuelPacket p;
    p.header.packetID   = id;
    p.header.type       = FUEL_STATUS;
    p.header.aircraftID = aircraftID;
    p.header.timestamp  = 1700000000; // fixed for deterministic tests

    p.body.fuelLevel           = fuel;
    p.body.consumptionRate     = 2.5f;
    p.body.flightTimeRemaining = 120.0f;
    p.body.nearestAirportID    = 5;
    p.body.emergencyFlag       = emergency;
    p.body.alertMessage        = const_cast<char*>(msg);
    return p;
}

namespace ClientTesting
{
   
    // UT-CLT-001 to UT-CLT-008  –  Packet Serialization / Deserialization
    // Requirements: REQ-PKT-010, REQ-PKT-020, REQ-PKT-050, REQ-PKT-060, REQ-PKT-070, REQ-SYS-010, REQ-SYS-030
  
    TEST_CLASS(PacketSerializationTests)
    {
    public:

        // UT-CLT-001
        TEST_METHOD(SerializePacket_ProducesNonEmptyString)
        {
            FuelPacket p = MakeClientTestPacket();
            std::string result = SerializePacket(p);
            Assert::IsFalse(result.empty(),
                L"Serialized string must not be empty");
        }

        // UT-CLT-002
        TEST_METHOD(SerializePacket_ContainsAircraftID)
        {
            FuelPacket p = MakeClientTestPacket(1, 202);
            std::string result = SerializePacket(p);
            Assert::IsTrue(result.find("202") != std::string::npos,
                L"Serialized string must contain aircraft ID 202");
        }

        // UT-CLT-003
        TEST_METHOD(SerializePacket_EmergencyFlagIsEncoded)
        {
            FuelPacket p = MakeClientTestPacket(1, 101, 10.0f, true, "LOW FUEL");
            std::string result = SerializePacket(p);
            // emergencyFlag=true serializes as the token "1"
            Assert::IsTrue(result.find("|1|") != std::string::npos,
                L"Emergency flag true must appear as '1' in serialized output");
        }

        // UT-CLT-004
        TEST_METHOD(DeserializePacket_RoundTrip_AircraftID)
        {
            FuelPacket original = MakeClientTestPacket(7, 303);
            std::string serialized = SerializePacket(original);
            FuelPacket restored = DeserializePacket(serialized);
            Assert::AreEqual(303, restored.header.aircraftID,
                L"Aircraft ID must survive a serialize/deserialize round-trip");
            delete[] restored.body.alertMessage;
        }

        // UT-CLT-005
        TEST_METHOD(DeserializePacket_RoundTrip_FuelLevel)
        {
            FuelPacket original = MakeClientTestPacket(2, 101, 55.5f);
            std::string serialized = SerializePacket(original);
            FuelPacket restored = DeserializePacket(serialized);
            Assert::AreEqual(55.5f, restored.body.fuelLevel, 0.001f,
                L"Fuel level must survive a serialize/deserialize round-trip");
            delete[] restored.body.alertMessage;
        }

        // UT-CLT-006
        TEST_METHOD(DeserializePacket_RoundTrip_AlertMessage)
        {
            FuelPacket original = MakeClientTestPacket(3, 101, 75.0f, false, "Nominal");
            std::string serialized = SerializePacket(original);
            FuelPacket restored = DeserializePacket(serialized);
            Assert::AreEqual(std::string("Nominal"),
                std::string(restored.body.alertMessage),
                L"Alert message must survive a serialize/deserialize round-trip");
            delete[] restored.body.alertMessage;
        }

        // UT-CLT-007
        TEST_METHOD(DeserializePacket_RoundTrip_EmergencyFlag_True)
        {
            FuelPacket original = MakeClientTestPacket(4, 101, 5.0f, true, "LOW FUEL");
            std::string serialized = SerializePacket(original);
            FuelPacket restored = DeserializePacket(serialized);
            Assert::IsTrue(restored.body.emergencyFlag,
                L"Emergency flag true must survive a round-trip");
            delete[] restored.body.alertMessage;
        }

        // UT-CLT-008
        TEST_METHOD(DeserializePacket_RoundTrip_NearestAirportID)
        {
            FuelPacket original = MakeClientTestPacket(5, 101, 75.0f);
            original.body.nearestAirportID = 42;
            std::string serialized = SerializePacket(original);
            FuelPacket restored = DeserializePacket(serialized);
            Assert::AreEqual(42, restored.body.nearestAirportID,
                L"Nearest airport ID must survive round-trip");
            delete[] restored.body.alertMessage;
        }
    };

    
    // UT-CLT-009 to UT-CLT-010  –  MessageType String Conversion
    // Requirements: REQ-SYS-050
  
    TEST_CLASS(MessageTypeStringTests)
    {
    public:

        // UT-CLT-009
        TEST_METHOD(MsgTypeToStr_AllKnownTypes_ReturnCorrectStrings)
        {
            Assert::AreEqual(std::string("HELLO"),        msgTypeToStr(MSG_HELLO));
            Assert::AreEqual(std::string("CHALLENGE"),    msgTypeToStr(MSG_CHALLENGE));
            Assert::AreEqual(std::string("RESPONSE"),     msgTypeToStr(MSG_RESPONSE));
            Assert::AreEqual(std::string("VERIFY_OK"),    msgTypeToStr(MSG_VERIFY_OK));
            Assert::AreEqual(std::string("VERIFY_FAIL"),  msgTypeToStr(MSG_VERIFY_FAIL));
            Assert::AreEqual(std::string("NACK"),         msgTypeToStr(MSG_NACK));
            Assert::AreEqual(std::string("REQ_SNAPSHOT"), msgTypeToStr(MSG_REQ_SNAPSHOT));
            Assert::AreEqual(std::string("ACK"),          msgTypeToStr(MSG_ACK));
            Assert::AreEqual(std::string("DATA"),         msgTypeToStr(MSG_DATA));
            Assert::AreEqual(std::string("FILE_DONE"),    msgTypeToStr(MSG_FILE_DONE));
            Assert::AreEqual(std::string("REQ_CLOSE"),    msgTypeToStr(MSG_REQ_CLOSE));
            Assert::AreEqual(std::string("REQ_DOWNLOAD"), msgTypeToStr(MSG_REQ_DOWNLOAD));
            Assert::AreEqual(std::string("FILE_CHUNK"),   msgTypeToStr(MSG_FILE_CHUNK));
        }

        // UT-CLT-010
        TEST_METHOD(MsgTypeToStr_UnknownType_ReturnsUnknown)
        {
            Assert::AreEqual(std::string("UNKNOWN"), msgTypeToStr(0xFF),
                L"MSG_UNKNOWN (0xFF) must return 'UNKNOWN'");
        }
    };

    // =======================================================================
    // UT-CLT-011 to UT-CLT-012  –  Protocol Header Structure
    // Requirements: REQ-SYS-010
    // =======================================================================
    TEST_CLASS(ProtocolHeaderStructureTests)
    {
    public:

        // UT-CLT-011
        TEST_METHOD(ProtocolHeader_SizeIs12Bytes)
        {
            // 4 (magic) + 1 (version) + 1 (msgType) + 2 (seq) + 4 (payloadLen) = 12
            Assert::AreEqual((size_t)12, sizeof(ProtocolHeader),
                L"ProtocolHeader must be exactly 12 bytes");
        }

        // UT-CLT-012
        TEST_METHOD(ProtocolMagic_CorrectValue)
        {
            Assert::AreEqual((uint32_t)0xDEAD1234, PROTO_MAGIC,
                L"Protocol magic constant must be 0xDEAD1234");
        }
    };

    // UT-CLT-013 to UT-CLT-015  –  Logger
    // Requirements: REQ-SYS-050
    TEST_CLASS(LoggerTests)
    {
    public:

        // UT-CLT-013
        TEST_METHOD(Logger_Open_CreatesFile)
        {
            const std::string testFile = "test_clt_logger_open.txt";
            remove(testFile.c_str());

            ::Logger logger;
            logger.open(testFile);
            logger.log("TX", "HELLO", 1, 5);

            std::ifstream f(testFile);
            Assert::IsTrue(f.good(),
                L"Log file must exist after Logger::open() and log()");
            f.close();
            remove(testFile.c_str());
        }

        // UT-CLT-014
        TEST_METHOD(Logger_Log_EntryContainsRequiredFields)
        {
            const std::string testFile = "test_clt_logger_entry.txt";
            remove(testFile.c_str());

            ::Logger logger;
            logger.open(testFile);
            logger.log("RX", "DATA", 3, 128);

            std::ifstream f(testFile);
            std::string line;
            std::getline(f, line);
            f.close();
            remove(testFile.c_str());

            Assert::IsTrue(line.find("RX")    != std::string::npos, L"Entry must contain 'RX'");
            Assert::IsTrue(line.find("DATA")  != std::string::npos, L"Entry must contain 'DATA'");
            Assert::IsTrue(line.find("seq=3") != std::string::npos, L"Entry must contain 'seq=3'");
            Assert::IsTrue(line.find("len=128") != std::string::npos, L"Entry must contain 'len=128'");
        }

        // UT-CLT-015
        TEST_METHOD(GetMsgType_ExtractsFirstToken)
        {
            std::string msg = "STATE|Verified|0|0";
            Assert::AreEqual(std::string("STATE"), getMsgType(msg),
                L"getMsgType must return the token before the first '|'");
        }
    };

    // UT-CLT-016  –  PacketType Enum
    // Requirements: REQ-PKT-010
    TEST_CLASS(PacketTypeEnumTests)
    {
    public:

        // UT-CLT-016
        TEST_METHOD(PacketType_EnumValues_AreDistinct)
        {
            Assert::AreNotEqual((int)FUEL_STATUS, (int)LANDED_SAFE,
                L"FUEL_STATUS and LANDED_SAFE must differ");
            Assert::AreNotEqual((int)LANDED_SAFE, (int)ACK_DIVERT,
                L"LANDED_SAFE and ACK_DIVERT must differ");
            Assert::AreNotEqual((int)FUEL_STATUS, (int)ACK_DIVERT,
                L"FUEL_STATUS and ACK_DIVERT must differ");
        }
    };
}
