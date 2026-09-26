#include "ParkingSystem.h"
#include "WebServer.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1024 || port > 65535) throw std::exception();
        } catch (...) {
            std::cerr << "Usage: parking_system [port 1024-65535]\n";
            return 1;
        }
    }

    try {
        ParkingSystem system;
        WebServer server(system, port);
        server.start();
    } catch (const std::exception& error) {
        std::cerr << "System error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
