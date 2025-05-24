#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <string>

class RoomClient {

public:
	RoomClient(const std::string& host, const std::string& port);

	void connect();
	void send(const std::string& message);

private:
	void do_read();

	boost::asio::io_context io_context_;
	boost::asio::ip::tcp::resolver resolver_;
	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
	boost::beast::flat_buffer buffer_;
	std::thread io_thread_;

};