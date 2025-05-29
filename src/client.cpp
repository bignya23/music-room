#include "headers/client.hpp"
#include <iostream>
#include "headers/music.hpp"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

RoomClient::RoomClient(const std::string& host, const std::string& port)
    : resolver_(io_context_), ws_(io_context_) {

    auto const results = resolver_.resolve(host, port);
    boost::asio::connect(ws_.next_layer(), results.begin(), results.end());
    ws_.handshake(host, "/");
    std::cout << "Connected to server." << std::endl;

    // Start audio processing thread
    audio_thread_ = std::thread(&RoomClient::audio_processing_loop, this);

    do_read();
    io_thread_ = std::thread([this]() {
        io_context_.run();
        });
}

RoomClient::~RoomClient() {
    stop();
}

void RoomClient::stop() {
    should_stop_ = true;

    // Wake up audio thread
    audio_cv_.notify_all();

    // Stop io_context
    io_context_.stop();

    // Join threads
    if (audio_thread_.joinable()) {
        audio_thread_.join();
    }
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

// Sending message from the client
void RoomClient::send(const std::string& message) {
    boost::asio::post(io_context_, [this, message]() {
        bool is_writing = !write_queue_.empty();
        write_queue_.push(message);
        if (!is_writing) {
            do_write();
        }
        });
}

// Reading messages from the server 
void RoomClient::do_read() {
    ws_.async_read(buffer_, [this](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            auto data = buffer_.data();
            std::string received = boost::beast::buffers_to_string(data);
            buffer_.consume(bytes_transferred);

            try {
                auto j = json::parse(received);
                std::string type = j.value("type", "");

                if (type == "audio") {
                    std::string base64Chunk = j.value("data", "");

                    // Queue audio data for processing in separate thread
                    {
                        std::lock_guard<std::mutex> lock(audio_mutex_);
                        audio_queue_.push(base64Chunk);
                    }
                    audio_cv_.notify_one();
                }
                else if (type == "text") {
                    std::string msg = j.value("data", "");
                    std::cout << "[Chat] " << msg << std::endl;
                }
                else {
                    std::cerr << "[Warning] Unknown message type: " << type << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[Error] Failed to parse JSON: " << e.what() << "\nRaw: " << received << std::endl;
            }

            do_read(); // Continue reading
        }
        else {
            std::cerr << "Read error: " << ec.message() << std::endl;
            should_stop_ = true;
        }
        });
}

// Writing messages to the server 
void RoomClient::do_write() {
    if (write_queue_.empty()) {
        return;
    }

    auto message_ptr = std::make_shared<std::string>(std::move(write_queue_.front()));
    write_queue_.pop();

    ws_.async_write(boost::asio::buffer(*message_ptr),
        [this, message_ptr](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                if (!write_queue_.empty()) {
                    do_write();
                }
            }
            else {
                std::cerr << "Write error: " << ec.message() << std::endl;
                write_queue_ = {};
            }
        });
}

// Audio processing in separate thread
void RoomClient::audio_processing_loop() {
    // Initialize audio system once in this thread
    music::initializeAudio();

    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(audio_mutex_);
        audio_cv_.wait(lock, [this] { return !audio_queue_.empty() || should_stop_; });

        while (!audio_queue_.empty() && !should_stop_) {
            std::string base64Chunk = audio_queue_.front();
            audio_queue_.pop();
            lock.unlock();

            // Process audio chunk
            try {
                music::handleAudioChunk(base64Chunk);
            }
            catch (const std::exception& e) {
                std::cerr << "[Audio Error] " << e.what() << std::endl;
            }

            lock.lock();
        }
    }

    // Cleanup audio system
    music::cleanupAudio();
}

void RoomClient::sendTextMessage(const std::string& msg) {
    nlohmann::json j = {
        {"type", "text"},
        {"data", msg}
    };
    send(j.dump());
}

void RoomClient::sendAudioChunk(const std::string& base64Chunk) {
    nlohmann::json j = {
        {"type", "audio"},
        {"data", base64Chunk}
    };
    send(j.dump());
}

void RoomClient::streamAudioFile(const std::string& filepath) {
    // Stream audio file and send chunks via WebSocket
    std::thread([this, filepath]() {
        music::streamAudioFile(filepath, [this](const std::string& base64Chunk) {
            if (!should_stop_) {
                sendAudioChunk(base64Chunk);
            }
            });
        }).detach();
}