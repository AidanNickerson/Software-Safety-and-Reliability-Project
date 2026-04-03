// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#pragma once
#include <fstream>
#include <string>
#include <ctime>

// Returns the message type token
inline std::string getMsgType(const std::string& msg) {
    auto pos = msg.find('|');
    return (pos == std::string::npos) ? msg : msg.substr(0, pos);
}

// Lightweight file-based packet logger.
class Logger {
public:
    Logger() = default;
    ~Logger() { if (logFile.is_open()) logFile.close(); }

    void open(const std::string& filename) {
        logFile.open(filename, std::ios::app);
    }

    // direction
    // msgType   
    // seq
    // payloadLen
    void log(const std::string& direction,
             const std::string& msgType,
             int seq,
             size_t payloadLen)
    {
        if (!logFile.is_open()) return;

        time_t now = time(nullptr);
        struct tm t;
        localtime_s(&t, &now);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);

        logFile << buf
                << " | " << direction
                << " | " << msgType
                << " | seq=" << seq
                << " | len=" << payloadLen
                << "\n";
        logFile.flush();
    }

private:
    std::ofstream logFile;
};
