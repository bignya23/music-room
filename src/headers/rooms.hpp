#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json.hpp>
#include <iostream>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
using WebSocketPtr = std::shared_ptr<boost::beast::websocket::stream<tcp::socket>>;

class RoomManager {
public:
    std::string createRoom(const std::string& room_id, WebSocketPtr client);
    std::string joinRoom(const std::string& room_id, WebSocketPtr client);
    void removeClient(WebSocketPtr client);
    std::string getClientRoom(WebSocketPtr client);
    std::vector<WebSocketPtr> getRoomClients(const std::string& room_id);
    bool isRoomFull(const std::string& room_id);
    void lockRoom(const std::string& room_id);
    bool isRoomLocked(const std::string& room_id);

private:
    std::unordered_map<std::string, std::vector<WebSocketPtr>> rooms_;
    std::unordered_map<WebSocketPtr, std::string> client_room_map_;
    std::unordered_map<std::string, bool> room_locks_;

    std::unordered_map<std::string, WebSocketPtr> room_hosts_;
    std::unordered_map<std::string, WebSocketPtr> current_djs_;
    std::unordered_map<std::string, std::unordered_map<WebSocketPtr, int>> votes_;


    std::mutex mutex_;
};
