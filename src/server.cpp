#include "headers/server.hpp"

RoomServer::RoomServer(boost::asio::io_context& ioc, unsigned short port)
    : io_context_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
}

void RoomServer::run() {
    do_accept();
}

void RoomServer::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));

            // Set WebSocket options for better stability
            ws->set_option(websocket::stream_base::timeout::suggested(
                boost::beast::role_type::server));
            ws->set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(boost::beast::http::field::server, "RoomServer");
                }));

            ws->async_accept([this, ws](boost::system::error_code ec) {
                if (!ec) {
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        client_ids_[ws] = next_client_no_;
                        next_client_no_++;
                        clients_.insert(ws);
                        client_queues_[ws] = std::make_shared<ClientMessageQueue>();
                        std::cout << "New client joined! No of clients : " << clients_.size() << std::endl;
                    }
                    handle_client(ws);
                }
                else {
                    std::cout << "WebSocket handshake failed: " << ec.message() << std::endl;
                }
                });
        }
        else {
            std::cout << "Accept failed: " << ec.message() << std::endl;
        }
        do_accept();
        });
}

void RoomServer::handle_client(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    auto buffer = std::make_shared<boost::beast::flat_buffer>();

    ws->async_read(*buffer, [this, ws, buffer](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            try {
                auto data = buffer->data();
                std::string msg = boost::beast::buffers_to_string(data);
                buffer->consume(bytes_transferred);

                std::cout << "Message from client: " << msg << std::endl;

                // Parse and handle message
                parse_message(msg, ws);

                // Continue reading
                handle_client(ws);
            }
            catch (const std::exception& e) {
                std::cout << "Error processing message: " << e.what() << std::endl;
                remove_client(ws);
            }
        }
        else if (ec != websocket::error::closed) {
            std::cout << "Client disconnected. Error: " << ec.message() << std::endl;
            remove_client(ws);
        }
        else {
            std::cout << "Client disconnected gracefully." << std::endl;
            remove_client(ws);
        }
        });
}

void RoomServer::parse_message(const std::string& message, std::shared_ptr<websocket::stream<tcp::socket>> sender) {
    try {
        auto j = json::parse(message);

        if (!j.contains("type")) {
            std::cout << "Message missing 'type' field" << std::endl;
            return;
        }

        std::string type = j["type"];

        if (type == "join") {
            if (!j.contains("room")) {
                queue_message_for_client(sender, R"({"type":"error","data":"Missing room field"})");
                return;
            }
            std::string room_id = j["room"];
            {
                std::lock_guard<std::mutex> lock(rooms_mutex_);
                std::string response_msg = room_manager_.joinRoom(room_id, sender);
                queue_message_for_client(sender, response_msg);
                std::cout << response_msg << std::endl;
            }
        }
        else if (type == "create") {
            if (!j.contains("room")) {
                queue_message_for_client(sender, R"({"type":"error","data":"Missing room field"})");
                return;
            }
            std::string room_id = j["room"];
            {
                std::lock_guard<std::mutex> lock(rooms_mutex_);
                std::string response_msg = room_manager_.createRoom(room_id, sender);
                queue_message_for_client(sender, response_msg);
                std::cout << response_msg << std::endl;
            }
        }
        else if (type == "audio") {  // Changed from "music_chunk" to match client
            if (!j.contains("data")) {
                std::cout << "Audio message missing data field" << std::endl;
                return;
            }

            std::string room_id;
            std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients_to_send;

            {
                std::lock_guard<std::mutex> lock(rooms_mutex_);
                room_id = room_manager_.getClientRoom(sender);
                if (room_id.empty()) {
                    std::cout << "Client not in any room" << std::endl;
                    return;
                }

                auto clients = room_manager_.getRoomClients(room_id);
                for (auto& client : clients) {
                    if (client != sender) { // Don't send back to sender
                        clients_to_send.push_back(client);
                    }
                }
            }

            // Forward audio to all other clients in the room
            json audio_msg = {
                {"type", "audio"},
                {"data", j["data"]}
            };

            for (auto& client : clients_to_send) {
                queue_message_for_client(client, audio_msg.dump());
            }
        }
        else if (type == "text") {
            // Handle text messages
            broadcast_message(message, sender);
        }
        else {
            std::cout << "Unknown message type: " << type << std::endl;
            // Still try to broadcast unknown messages
            broadcast_message(message, sender);
        }
    }
    catch (const json::parse_error& e) {
        std::cout << "JSON parse error: " << e.what() << std::endl;
        // Try to broadcast as plain text
        broadcast_message(message, sender);
    }
    catch (const std::exception& e) {
        std::cout << "Error parsing message: " << e.what() << std::endl;
    }
}

void RoomServer::broadcast_message(const std::string& message, std::shared_ptr<websocket::stream<tcp::socket>> sender) {
    std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients_to_send;

    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        std::string room_id = room_manager_.getClientRoom(sender);
        if (room_id.empty()) {
            std::cout << "Sender not in any room, cannot broadcast" << std::endl;
            return;
        }

        std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients = room_manager_.getRoomClients(room_id);
        for (const auto& client : clients) {
            if (client != sender) {
                clients_to_send.push_back(client);
            }
        }
    }

    for (auto& client : clients_to_send) {
        queue_message_for_client(client, message);
    }
}

void RoomServer::queue_message_for_client(std::shared_ptr<websocket::stream<tcp::socket>> client, const std::string& message) {
    std::shared_ptr<ClientMessageQueue> queue;

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = client_queues_.find(client);
        if (it == client_queues_.end()) {
            std::cout << "Client queue not found" << std::endl;
            return;
        }
        queue = it->second;
    }

    std::lock_guard<std::mutex> lock(queue->mtx);
    bool is_empty = queue->messages.empty();
    queue->messages.push(message);

    if (is_empty) {
        // Post to IO context to ensure thread safety
        boost::asio::post(io_context_, [this, client, queue]() {
            send_next_message(client, queue);
            });
    }
}

void RoomServer::send_next_message(std::shared_ptr<websocket::stream<tcp::socket>> client, std::shared_ptr<ClientMessageQueue> queue) {
    // Check if client still exists
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.find(client) == clients_.end()) {
            std::cout << "Client no longer exists, skipping message send" << std::endl;
            return;
        }
    }

    auto message_ptr = std::make_shared<std::string>();

    {
        std::lock_guard<std::mutex> lock(queue->mtx);
        if (queue->messages.empty()) {
            return;
        }
        *message_ptr = std::move(queue->messages.front());
        queue->messages.pop();
    }

    // Ensure WebSocket is in text mode for JSON messages
    client->text(true);

    client->async_write(boost::asio::buffer(*message_ptr),
        [this, client, queue, message_ptr](boost::system::error_code ec, std::size_t bytes_written) {
            if (ec) {
                if (ec != websocket::error::closed) {
                    std::cout << "Failed to send message to client: " << ec.message() << std::endl;
                }
                remove_client(client);
            }
            else {
                // Check if there are more messages to send
                bool has_more_messages = false;
                {
                    std::lock_guard<std::mutex> queue_lock(queue->mtx);
                    has_more_messages = !queue->messages.empty();
                }

                if (has_more_messages) {
                    // Post to IO context to continue sending
                    boost::asio::post(io_context_, [this, client, queue]() {
                        send_next_message(client, queue);
                        });
                }
            }
        });
}

void RoomServer::remove_client(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(ws);
        client_queues_.erase(ws);
        client_ids_.erase(ws);
    }

    // Remove from rooms
    {
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        room_manager_.removeClient(ws);
    }

    // Close the WebSocket connection gracefully
    boost::system::error_code ec;
    ws->close(websocket::close_code::normal, ec);

    std::cout << "Client removed. Remaining clients: " << clients_.size() << std::endl;
}