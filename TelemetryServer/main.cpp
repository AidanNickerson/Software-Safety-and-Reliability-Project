// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#include "Server.h"

int main() {
    Server server;

    if (!server.start(8080)) {
        return 1;
    }

    server.run();

    return 0;
}