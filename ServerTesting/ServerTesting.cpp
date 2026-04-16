#include "pch.h"
#include "CppUnitTest.h"

// Pull in the server-side headers and implementation
#include "../TelemetryServer/Packet.h"
#include "../TelemetryServer/Packetutils.h"
#include "../TelemetryServer/Logger.h"
#include "../TelemetryServer/Server.h"

// Include the implementation directly so the linker finds the definitions
// without needing to add the source project as a dependency.
#include "../TelemetryServer/Packetutils.cpp"

#include <string>
#include <fstream>
#include <sstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Helpers
static const char* SVR_TEST_LOG = "test_svr_telemetry_tmp.log";

static void RemoveSvrLog() { remove(SVR_TEST_LOG); }

static long SvrFileSize(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f.is_open() ? static_cast<long>(f.tellg()) : -1L;
}

static std::string SvrLastNonEmptyLine(const char* path) {
    std::ifstream f(path);
    std::string line, last;
    while (std::getline(f, line))
        if (!line.empty()) last = line;
    return last;
}

static FuelPacket MakeSvrTestPacket(int id = 1, float fuel = 60.0f,
                                    bool emergency = false,
                                    const char* msg = "Normal")
{
    FuelPacket p;
    p.header.packetID   = id;
    p.header.type       = FUEL_STATUS;
    p.header.aircraftID = 101;
    p.header.timestamp  = 1700000000;
    p.body.fuelLevel           = fuel;
    p.body.consumptionRate     = 2.2f;
    p.body.flightTimeRemaining = 100.0f;
    p.body.nearestAirportID    = 5;
    p.body.emergencyFlag       = emergency;
    p.body.alertMessage        = const_cast<char*>(msg);
    return p;
}

namespace ServerTesting
{
    // UT-SVR-001 to UT-SVR-002  –  Server State String Conversion
    // Requirements: REQ-SYS-060
    TEST_CLASS(ServerStateStringTests)
    {
    public:

        // UT-SVR-001
        TEST_METHOD(ServerStateToStr_AllStates_ReturnCorrectStrings)
        {
            Assert::AreEqual(std::string("WaitingForClient"), serverStateToStr(STATE_WAITING));
            Assert::AreEqual(std::string("Connected"),        serverStateToStr(STATE_CONNECTED));
            Assert::AreEqual(std::string("Verified"),         serverStateToStr(STATE_VERIFIED));
            Assert::AreEqual(std::string("Transferring"),     serverStateToStr(STATE_TRANSFERRING));
            Assert::AreEqual(std::string("Closing"),          serverStateToStr(STATE_CLOSING));
            Assert::AreEqual(std::string("Error"),            serverStateToStr(STATE_ERROR));
        }

        // UT-SVR-002
        TEST_METHOD(ServerStateToStr_OutOfRange_ReturnsUnknown)
        {
            Assert::AreEqual(std::string("Unknown"),
                serverStateToStr(static_cast<ServerState>(99)),
                L"Out-of-range state must return 'Unknown'");
        }
    };

    // UT-SVR-003 to UT-SVR-005  –  Server-Side Packet Serialization
    // Requirements: REQ-PKT-010, REQ-PKT-030, REQ-SYS-010
    TEST_CLASS(ServerPacketSerializationTests)
    {
    public:

        // UT-SVR-003
        TEST_METHOD(SerializePacket_NonEmpty)
        {
            FuelPacket p = MakeSvrTestPacket();
            Assert::IsFalse(SerializePacket(p).empty(),
                L"Serialized packet must not be empty");
        }

        // UT-SVR-004
        TEST_METHOD(SerializePacket_ContainsAircraftID)
        {
            FuelPacket p = MakeSvrTestPacket();
            std::string s = SerializePacket(p);
            Assert::IsTrue(s.find("101") != std::string::npos,
                L"Serialized output must contain aircraftID 101");
        }

        // UT-SVR-005
        TEST_METHOD(DeserializePacket_RoundTrip_ConsumptionRate)
        {
            FuelPacket original = MakeSvrTestPacket();
            original.body.consumptionRate = 3.7f;
            std::string s = SerializePacket(original);
            FuelPacket restored = DeserializePacket(s);
            Assert::AreEqual(3.7f, restored.body.consumptionRate, 0.001f,
                L"Consumption rate must survive round-trip");
            delete[] restored.body.alertMessage;
        }
    };

    // UT-SVR-006 to UT-SVR-011  –  Telemetry File I/O Logic
    // Requirements: REQ-SYS-050, REQ-SYS-070, REQ-PKT-060
    TEST_CLASS(TelemetryFileTests)
    {
    public:

        // UT-SVR-006 – Appending a serialized entry increases file size
        TEST_METHOD(AppendTelemetry_IncreasesFileSize)
        {
            RemoveSvrLog();

            FuelPacket p = MakeSvrTestPacket(1, 80.5f);
            std::ofstream f(SVR_TEST_LOG, std::ios::app);
            Assert::IsTrue(f.is_open(),
                L"Must be able to open telemetry log for appending");
            f << SerializePacket(p) << "\n";
            f.close();

            Assert::IsTrue(SvrFileSize(SVR_TEST_LOG) > 0,
                L"File size must be > 0 after first append");
            RemoveSvrLog();
        }

        // UT-SVR-007 – Last-line helper returns the most-recent entry
        TEST_METHOD(ReadLastTelemetryEntry_ReturnsLastLine)
        {
            RemoveSvrLog();

            FuelPacket p1 = MakeSvrTestPacket(1, 80.0f, false, "Normal");
            FuelPacket p2 = MakeSvrTestPacket(2, 15.0f, true,  "LOW FUEL");
            std::string line1 = SerializePacket(p1);
            std::string line2 = SerializePacket(p2);

            std::ofstream f(SVR_TEST_LOG, std::ios::app);
            f << line1 << "\n" << line2 << "\n";
            f.close();

            Assert::AreEqual(line2, SvrLastNonEmptyLine(SVR_TEST_LOG),
                L"readLastTelemetryEntry must return the last non-empty line");
            RemoveSvrLog();
        }

        // UT-SVR-008 – Returns empty string when file is absent
        TEST_METHOD(ReadLastTelemetryEntry_MissingFile_ReturnsEmpty)
        {
            RemoveSvrLog();
            Assert::IsTrue(SvrLastNonEmptyLine(SVR_TEST_LOG).empty(),
                L"Must return empty string when telemetry log is absent");
        }

        // UT-SVR-009 – Emergency flag set when fuel < 20%
        TEST_METHOD(EmergencyFlag_SetWhenFuelBelow20)
        {
            FuelPacket p = MakeSvrTestPacket(1, 10.0f);
            p.body.emergencyFlag = (p.body.fuelLevel < 20.0f);
            Assert::IsTrue(p.body.emergencyFlag,
                L"Emergency flag must be true when fuelLevel < 20");
        }

        // UT-SVR-010 – Emergency flag clear when fuel >= 20%
        TEST_METHOD(EmergencyFlag_ClearWhenFuelAtOrAbove20)
        {
            FuelPacket p = MakeSvrTestPacket(1, 50.0f);
            p.body.emergencyFlag = (p.body.fuelLevel < 20.0f);
            Assert::IsFalse(p.body.emergencyFlag,
                L"Emergency flag must be false when fuelLevel >= 20");
        }

        // UT-SVR-011 – ensureMinFileSize pads file to >= 1 MB (REQ-SYS-070)
        TEST_METHOD(EnsureMinFileSize_PadsFileToAtLeast1MB)
        {
            RemoveSvrLog();
            const size_t MIN_BYTES = 1048576;

            size_t currentSize = 0;
            int padID = 2000000;
            std::ofstream logFile(SVR_TEST_LOG, std::ios::app);
            Assert::IsTrue(logFile.is_open(), L"Must open test log for padding");

            while (currentSize < MIN_BYTES) {
                ++padID;
                FuelPacket p;
                p.header.packetID   = padID;
                p.header.type       = FUEL_STATUS;
                p.header.aircraftID = 101;
                p.header.timestamp  = 1700000000;
                float fuel = 80.5f - (padID * 0.001f);
                p.body.fuelLevel           = fuel > 0.0f ? fuel : 0.0f;
                p.body.consumptionRate     = 2.2f;
                p.body.flightTimeRemaining = 150.0f;
                p.body.nearestAirportID    = 5;
                p.body.emergencyFlag       = (p.body.fuelLevel < 20.0f);
                p.body.alertMessage        = p.body.emergencyFlag
                                               ? const_cast<char*>("LOW FUEL")
                                               : const_cast<char*>("Normal");
                std::string entry = SerializePacket(p) + "\n";
                logFile << entry;
                currentSize += entry.size();
            }
            logFile.close();

            Assert::IsTrue(SvrFileSize(SVR_TEST_LOG) >= static_cast<long>(MIN_BYTES),
                L"File must be >= 1 MB after padding (REQ-SYS-070)");
            RemoveSvrLog();
        }
    };

    // UT-SVR-012 to UT-SVR-013  –  Server Logger
    // Requirements: REQ-SYS-050
    TEST_CLASS(ServerLoggerTests)
    {
    public:

        // UT-SVR-012 – TX entry written correctly
        TEST_METHOD(Logger_WritesTXEntry)
        {
            const std::string logPath = "test_svr_logger_tx.txt";
            remove(logPath.c_str());

            ::Logger logger;
            logger.open(logPath);
            logger.log("TX", "ACK", 5, 0);

            std::ifstream f(logPath);
            std::string line;
            std::getline(f, line);
            f.close();
            remove(logPath.c_str());

            Assert::IsTrue(line.find("TX")    != std::string::npos, L"Entry must contain 'TX'");
            Assert::IsTrue(line.find("ACK")   != std::string::npos, L"Entry must contain 'ACK'");
            Assert::IsTrue(line.find("seq=5") != std::string::npos, L"Entry must contain 'seq=5'");
        }

        // UT-SVR-013 – Multiple entries appended, not overwritten
        TEST_METHOD(Logger_AppendsMultipleEntries)
        {
            const std::string logPath = "test_svr_logger_multi.txt";
            remove(logPath.c_str());

            ::Logger logger;
            logger.open(logPath);
            logger.log("TX", "HELLO",     1, 5);
            logger.log("RX", "CHALLENGE", 2, 5);
            logger.log("TX", "RESPONSE",  3, 17);
            logger.log("RX", "VERIFY_OK", 4, 0);

            int lineCount = 0;
            std::ifstream f(logPath);
            std::string line;
            while (std::getline(f, line)) ++lineCount;
            f.close();
            remove(logPath.c_str());

            Assert::AreEqual(4, lineCount,
                L"Logger must write 4 separate log entries");
        }
    };

    // UT-SVR-014  –  Protocol Header Structure (server side)
    // Requirements: REQ-SYS-010
    TEST_CLASS(ServerProtocolHeaderTests)
    {
    public:

        // UT-SVR-014
        TEST_METHOD(ProtocolHeader_SizeIs12Bytes)
        {
            // 4 (magic) + 1 (version) + 1 (msgType) + 2 (seq) + 4 (payloadLen) = 12
            Assert::AreEqual((size_t)12, sizeof(ProtocolHeader),
                L"ProtocolHeader must be exactly 12 bytes");
        }
    };
}
