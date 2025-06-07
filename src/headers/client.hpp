#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <queue>
#include <thread>
#include <json.hpp>
#include <atomic>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

class RoomClient {
public:
    RoomClient(const std::string& host, const std::string& port);
    ~RoomClient();

    void send(const std::string& message);
    void sendTextMessage(const std::string& msg);
    void sendAudioChunk(const std::string& base64Chunk);
    void stop();

private:
    void do_read();
    void do_write();

    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    tcp::resolver resolver_;
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    std::queue<std::string> write_queue_;
    std::thread io_thread_;
    std::atomic<bool> stopped_{ false };
};