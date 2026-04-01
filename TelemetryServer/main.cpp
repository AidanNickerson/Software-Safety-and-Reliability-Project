#include "Server.h"

int main() {
    Server server;

    if (!server.start(8080)) {
        return 1;
    }

    server.run();

    return 0;
}