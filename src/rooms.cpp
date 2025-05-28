#include "headers/rooms.hpp"


std::string RoomManager::createRoom(const std::string& room_id, WebSocketPtr client)
{
    json response;

	std::lock_guard<std::mutex> lock(mutex_);
    if (rooms_.find(room_id) != rooms_.end()) {
        response["type"] = "error";
        response["message"] = "Room already exist";
    
    }
    else {
        response["type"] = "success";
        response["message"] = "Successfully created room.";

    }
    rooms_[room_id].push_back(client);
    client_room_map_[client] = room_id;
    room_hosts_[room_id] = client;
    room_locks_[room_id] = false;
	
    return response.dump();
}


std::string RoomManager::joinRoom(const std::string& room_id, WebSocketPtr client) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    json response;
    if (it != rooms_.end()) {
        it->second.push_back(client);
        client_room_map_[client] = room_id;
        response["type"] = "join_success";
        response["room"] = room_id;
    }
    else {
        response["type"] = "error";
        response["message"] = "Room does not exist";
    }
    return response.dump();
}

void RoomManager::removeClient(WebSocketPtr client) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = client_room_map_.find(client);
    if (it != client_room_map_.end()) {
        std::string room_id = it->second;
        auto& clients = rooms_[room_id];
        clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
        client_room_map_.erase(it);
    }
}

std::string RoomManager::getClientRoom(WebSocketPtr client) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = client_room_map_.find(client);
    return it != client_room_map_.end() ? it->second : "";
}

std::vector<WebSocketPtr> RoomManager::getRoomClients(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return rooms_[room_id];
}

//bool RoomManager::isRoomFull(const std::string& room_id) {
//    std::lock_guard<std::mutex> lock(mutex_);
//    return rooms_[room_id].size() >= 4; 
//}

void RoomManager::lockRoom(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    room_locks_[room_id] = true;
}

bool RoomManager::isRoomLocked(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return room_locks_.count(room_id) && room_locks_[room_id];
}
