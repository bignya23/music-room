// headers/client.hpp
#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <json.hpp>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
using json = nlohmann::json;

class RoomClient {
private:
    boost::asio::io_context io_context_;
    tcp::resolver resolver_;
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    std::queue<std::string> write_queue_;
    std::thread io_thread_;

    // Audio processing
    std::queue<std::string> audio_queue_;
    std::mutex audio_mutex_;
    std::condition_variable audio_cv_;
    std::thread audio_thread_;
    std::atomic<bool> should_stop_{ false };

    void do_read();
    void do_write();
    void audio_processing_loop();

public:
    RoomClient(const std::string& host, const std::string& port);
    ~RoomClient();

    void send(const std::string& message);
    void sendTextMessage(const std::string& msg);
    void sendAudioChunk(const std::string& base64Chunk);
    void streamAudioFile(const std::string& filepath);
    void stop();
};