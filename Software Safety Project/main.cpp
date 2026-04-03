// Amro Belbeisi, Aidan Nickerson, Mayank Kumar
// CSCN74000 - Software Safety and Reliability
// Group 8
#include "Client.h"

int main() {
    Client client;

    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    client.run();
    return 0;
}