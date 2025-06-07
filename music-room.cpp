#include "src/headers/client.hpp"
#include "src/headers/server.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "src/headers/music.hpp"

std::atomic<bool> should_exit{ false };
std::unique_ptr<RoomClient> global_client;

void signal_handler(int signal) {
    std::cout << "\nShutting down gracefully..." << std::endl;
    should_exit = true;
    if (global_client) {
        global_client->stop();
    }
}

int main() {
    // Set up signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

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
            global_client = std::make_unique<RoomClient>("34.59.107.23", "9000");

            int room_mode;
            std::string room_id;
            std::cout << "Enter mode (create(1)/join(2)): ";
            std::cin >> room_mode;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::cout << "Enter room id: ";
            std::getline(std::cin, room_id);

            if (room_mode == 1) {
                global_client->send(R"({"type":"create", "room":")" + room_id + R"("})");
            }
            else if (room_mode == 2) {
                global_client->send(R"({"type":"join", "room":")" + room_id + R"("})");
            }

            std::cout << "Connected! Type '/dj' to stream audio, '/quit' to exit." << std::endl;

            // Main input loop
            while (!should_exit) {
                std::string msg;
                std::cout << "Enter Message: ";

                if (!std::getline(std::cin, msg)) {
                    // EOF reached (Ctrl+D)
                    break;
                }

                if (should_exit) break;

                if (msg == "/quit" || msg == "/exit") {
                    break;
                }
                else if (msg == "/dj") {
                    std::cout << "Starting audio stream..." << std::endl;

                    // Stream audio in a separate thread to avoid blocking
                    std::thread audio_thread([&]() {
                        try {
                            music::streamAudioFile("C:\\Users\\bigny\\OneDrive\\Pictures\\sukoon.wav",
                                [&](const std::string& chunk) {
                                    if (!should_exit && global_client) {
                                        global_client->sendAudioChunk(chunk);
                                    }
                                });
                            std::cout << "Audio streaming completed." << std::endl;
                        }
                        catch (const std::exception& e) {
                            std::cerr << "Error in audio thread: " << e.what() << std::endl;
                        }
                        });

                    // Detach the thread so it runs independently
                    audio_thread.detach();
                    std::cout << "Audio streaming started in background. You can continue chatting." << std::endl;
                }
                else if (!msg.empty()) {
                    global_client->sendTextMessage(msg);
                }
            }

            std::cout << "Disconnecting..." << std::endl;
            global_client->stop();
            global_client.reset();
        }
        catch (const std::exception& e) {
            std::cerr << "Client error: " << e.what() << std::endl;
            if (global_client) {
                global_client->stop();
                global_client.reset();
            }
            return 1;
        }
    }
    else {
        std::cout << "Invalid mode. Please select 1 (Server) or 2 (Client)." << std::endl;
        return 1;
    }

    return 0;
}