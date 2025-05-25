#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <string>
#include <queue>

class RoomClient {

public:
	RoomClient(const std::string& host, const std::string& port);

	void connect();
	void send(const std::string& message);

private:
	void do_read();
	void do_write();

	boost::asio::io_context io_context_;
	boost::asio::ip::tcp::resolver resolver_;
	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
	boost::beast::flat_buffer buffer_;
	std::thread io_thread_;
	std::queue<std::string> write_queue_;


};