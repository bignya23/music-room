#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <set>


using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;


class RoomServer {
public:
    RoomServer(boost::asio::io_context& ioc, unsigned short port);
    void run();

private:
    void do_accept();
    void handle_client(std::shared_ptr<websocket::stream<tcp::socket>> ws);

    tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    std::set<std::shared_ptr<websocket::stream<tcp::socket>>> clients_;
};