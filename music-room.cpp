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


            int mode;
            std::string room_id;
            std::cout << "Enter mode (create(1)/join(2)): ";

            std::cin >> mode;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 

            std::cout << "Enter room id: ";

            std::getline(std::cin, room_id);



            if (mode == 1) {
                client.send(R"({"type":"create", "room":")" + room_id + R"("})");
            }
            else if (mode == 2) {
                client.send(R"({"type":"join", "room":")" + room_id + R"("})");

            }
            std::cout << "Connected!" << std::endl;

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