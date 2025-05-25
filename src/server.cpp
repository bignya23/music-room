#include <iostream>
#include <mutex>
#include <queue>
#include <memory>
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
            ws->async_accept([this, ws](boost::system::error_code ec) {
                if (!ec) {
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        clients_.insert(ws);
                        client_queues_[ws] = std::make_shared<ClientMessageQueue>();
                        std::cout << "New client joined! No of clients : " << clients_.size() << std::endl;
                    }
                    handle_client(ws);
                }
                });
        }
        do_accept();
        });
}

void RoomServer::handle_client(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    auto buffer = std::make_shared<boost::beast::flat_buffer>();
    ws->async_read(*buffer, [this, ws, buffer](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            auto data = buffer->data();
            std::string msg = boost::beast::buffers_to_string(data);
            buffer->consume(bytes_transferred);

            std::cout << "Message from client: " << msg << std::endl;
            // send message to all clients
            broadcast_message(msg, ws);

            handle_client(ws);
        }
        else {
            std::cout << "Client disconnected. Error: " << ec.message() << std::endl;
            remove_client(ws); // Use remove_client instead of direct erase
        }
        });
}

void RoomServer::broadcast_message(const std::string& message, std::shared_ptr<websocket::stream<tcp::socket>> sender) {
    std::vector<std::shared_ptr<websocket::stream<tcp::socket>>> clients_to_send;

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_to_send.reserve(clients_.size());
        for (const auto& client : clients_) {
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
            return;
        }
        queue = it->second;
    }

    std::lock_guard<std::mutex> lock(queue->mtx);
    bool is_empty = queue->messages.empty();
    queue->messages.push(message);

    if (is_empty) {
        // Post to io_context to avoid potential issues with calling from different threads
        boost::asio::post(io_context_, [this, client, queue]() {
            send_next_message(client, queue);
            });
    }
}

void RoomServer::send_next_message(std::shared_ptr<websocket::stream<tcp::socket>> client, std::shared_ptr<ClientMessageQueue> queue) {
    auto message_ptr = std::make_shared<std::string>();

    {
        std::lock_guard<std::mutex> lock(queue->mtx);
        if (queue->messages.empty()) {
            return;
        }
        *message_ptr = std::move(queue->messages.front());
        queue->messages.pop();
    }

    client->text(true);
    client->async_write(boost::asio::buffer(*message_ptr),
        [this, client, queue, message_ptr](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cout << "Failed to send message to client. Removing client. Error: " << ec.message() << std::endl;
                remove_client(client);
            }
            else {
                std::lock_guard<std::mutex> queue_lock(queue->mtx);
                if (!queue->messages.empty()) {
                    boost::asio::post(io_context_, [this, client, queue]() {
                        send_next_message(client, queue);
                        });
                }
            }
        });
}

void RoomServer::remove_client(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(ws);
    client_queues_.erase(ws);
    std::cout << "Remaining clients: " << clients_.size() << std::endl;
}