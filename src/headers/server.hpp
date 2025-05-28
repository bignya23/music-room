#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <set>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <json.hpp>
#include <iostream>
#include <memory>
#include "rooms.hpp"


using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;


class RoomServer {
public:
    
    RoomServer(boost::asio::io_context& ioc, unsigned short port);
    void run();

private:
    
    RoomManager room_manager_;  

    std::atomic<int> next_client_no_{ 1 };
   
    struct ClientMessageQueue {
        std::mutex mtx;
        std::queue<std::string> messages;
    };
    void do_accept();
    std::mutex clients_mutex_;
    std::mutex rooms_mutex_;
    std::unordered_map<std::shared_ptr<websocket::stream<tcp::socket>>, std::shared_ptr<ClientMessageQueue>> client_queues_;
    std::unordered_map<std::shared_ptr<websocket::stream<tcp::socket>>, int> client_ids_;

    void queue_message_for_client(std::shared_ptr<websocket::stream<tcp::socket>> client, const std::string& message);
    void handle_client(std::shared_ptr<websocket::stream<tcp::socket>> ws);
    void broadcast_message(const std::string& message, std::shared_ptr<websocket::stream<tcp::socket>> sender);
    void send_next_message(std::shared_ptr<websocket::stream<tcp::socket>> client, std::shared_ptr<ClientMessageQueue> queue);
    void remove_client(std::shared_ptr<websocket::stream<tcp::socket>> ws);
    void parse_message(const std::string& message, std::shared_ptr < websocket::stream <tcp::socket>> sender);

    tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    std::set<std::shared_ptr<websocket::stream<tcp::socket>>> clients_;
};