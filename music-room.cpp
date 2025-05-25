#include "src/headers/client.hpp"
#include "src/headers/server.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    int mode;
    std::cout << "Select mode:\n1. Server\n2. Client\nEnter choice: ";
    std::cin >> mode;

    if (mode == 1) {
        try {
            boost::asio::io_context io;
            RoomServer server(io, 9000);

            std::cout << "Starting server on port 9000..." << std::endl;
            server.run();

            std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
            io.run(); 
        }
        catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
            return 1;
        }
    }
    else if (mode == 2) {
        try {
            
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::cout << "Connecting to server..." << std::endl;
            RoomClient client("127.0.0.1", "9000");

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::cout << "Connected! Type messages (empty line to quit):" << std::endl;

            while (true) {
                std::string msg;
                std::getline(std::cin, msg);

                if (msg.empty()) {
                    std::cout << "Disconnecting..." << std::endl;
                    break;
                }

                client.send(msg);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Client error: " << e.what() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Invalid mode. Please select 1 (Server) or 2 (Client)." << std::endl;
        return 1;
    }

    return 0;
}