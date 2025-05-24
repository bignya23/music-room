#include <iostream>
#include "headers/server.hpp"


RoomServer::RoomServer(boost::asio::io_context& ioc, unsigned short port)
    : io_context_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
}

void RoomServer::run() {
    do_accept();
}

