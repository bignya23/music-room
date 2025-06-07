#include "headers/client.hpp"
#include <iostream>
#include "headers/music.hpp"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

RoomClient::RoomClient(const std::string& host, const std::string& port)
    : resolver_(io_context_), ws_(io_context_), work_guard_(boost::asio::make_work_guard(io_context_)) {

    try {
        auto const results = resolver_.resolve(host, port);
        boost::asio::connect(ws_.next_layer(), results.begin(), results.end());

        // Set WebSocket options for better stability
        ws_.set_option(websocket::stream_base::timeout::suggested(
            boost::beast::role_type::client));

        ws_.handshake(host, "/");
        std::cout << "Connected to server." << std::endl;

        do_read();

        // Start IO thread
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            }
            catch (const std::exception& e) {
                std::cerr << "IO thread error: " << e.what() << std::endl;
            }
            });
    }
    catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        throw;
    }
}

RoomClient::~RoomClient() {
    stop();
}

void RoomClient::stop() {
    if (stopped_.exchange(true)) {
        return; // Already stopped
    }

    std::cout << "Stopping client..." << std::endl;

    // Close WebSocket gracefully
    boost::asio::post(io_context_, [this]() {
        boost::system::error_code ec;
        if (ws_.is_open()) {
            ws_.close(websocket::close_code::normal, ec);
            if (ec) {
                std::cerr << "Error closing WebSocket: " << ec.message() << std::endl;
            }
        }
        });

    // Give some time for graceful close
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop IO context
    work_guard_.reset();
    io_context_.stop();

    // Join IO thread
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    std::cout << "Client stopped." << std::endl;
}

// Sending message from the client - THREAD SAFE with flow control
void RoomClient::send(const std::string& message) {
    if (stopped_.load()) return;

    boost::asio::post(io_context_, [this, message]() {
        if (stopped_.load()) return;

        // Check write queue size to prevent overwhelming
        if (write_queue_.size() > MAX_QUEUE_SIZE) {
            std::cerr << "Write queue full, dropping message" << std::endl;
            return;
        }

        bool is_writing = !write_queue_.empty();
        write_queue_.push(message);
        if (!is_writing) {
            do_write();
        }
        });
}

// Reading messages from the server 
void RoomClient::do_read() {
    if (stopped_.load()) return;

    auto self = shared_from_this();
    ws_.async_read(buffer_, [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (stopped_.load()) return;

        if (!ec) {
            try {
                auto data = buffer_.data();
                std::string received = boost::beast::buffers_to_string(data);
                buffer_.consume(bytes_transferred);

                auto j = json::parse(received);
                std::string type = j.value("type", "");

                if (type == "audio") {
                    std::string base64Chunk = j.value("data", "");
                    if (!base64Chunk.empty()) {
                        music::handleAudioChunk(base64Chunk);
                    }
                }
                else if (type == "text") {
                    std::string msg = j.value("data", "");
                    std::cout << "[Chat] " << msg << std::endl;
                }
                else if (type == "error") {
                    std::string error_msg = j.value("data", "Unknown error");
                    std::cout << "[Error] " << error_msg << std::endl;
                }
                else {
                    std::cout << "[Info] " << received << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[Parse Error] " << e.what() << "\nRaw: " << std::endl;
            }

            do_read(); // Continue reading
        }
        else if (ec == websocket::error::closed) {
            std::cout << "Connection closed by server." << std::endl;
        }
        else {
            std::cerr << "Read error: " << ec.message() << std::endl;
        }
        });
}

// Writing messages to the server with better error handling
void RoomClient::do_write() {
    if (stopped_.load() || write_queue_.empty()) {
        return;
    }

    auto message_ptr = std::make_shared<std::string>(std::move(write_queue_.front()));
    write_queue_.pop();

    auto self = shared_from_this();
    ws_.async_write(boost::asio::buffer(*message_ptr),
        [this, self, message_ptr](boost::system::error_code ec, std::size_t bytes_written) {
            if (stopped_.load()) return;

            if (!ec) {
                // Successfully sent, continue with next message if any
                if (!write_queue_.empty()) {
                    do_write();
                }
            }
            else if (ec == websocket::error::closed) {
                std::cout << "Connection closed during write." << std::endl;
            }
            else {
                std::cerr << "Write error: " << ec.message() << std::endl;
                // Clear queue on serious error
                std::queue<std::string> empty;
                write_queue_.swap(empty);
            }
        });
}

void RoomClient::sendTextMessage(const std::string& msg) {
    nlohmann::json j = {
        {"type", "text"},
        {"data", msg}
    };
    send(j.dump());
}

void RoomClient::sendAudioChunk(const std::string& base64Chunk) {
    if (base64Chunk.empty()) return;

    nlohmann::json j = {
        {"type", "audio"},
        {"data", base64Chunk}
    };
    send(j.dump());
}

std::shared_ptr<RoomClient> RoomClient::shared_from_this() {
    // This is a simple implementation - in practice you'd use std::enable_shared_from_this
    // For now, we'll just return a dummy shared_ptr
    return std::shared_ptr<RoomClient>(this, [](RoomClient*) {});
}