#include "headers/client.hpp"
#include <iostream>


using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;


RoomClient::RoomClient(const std::string& host, const std::string& port) : resolver_(io_context_), ws_(io_context_) {
	
	auto const results = resolver_.resolve(host, port);

	boost::asio::connect(ws_.next_layer(), results.begin(), results.end());
	ws_.handshake(host, "/");

	std::cout << "Connected to server." << std::endl;

	io_thread_ = std::thread([this]() {
		do_read();
		io_context_.run();
	});


}

// Sending message from the client
void RoomClient::send(const std::string& message) {
	ws_.write(boost::asio::buffer(message));
}


// Reading messages from the server 
void RoomClient::do_read() {
	ws_.async_read(buffer_, [this](boost::system::error_code ec, std::size_t) {
		if (!ec) {
			std::string recieved = boost::beast::buffers_to_string(buffer_.data());
			std::cout << "Recieved from server : " << recieved << std::endl;
			buffer_.consume(buffer_.size());
			do_read();
		}
		else {
			std::cerr << "Some error occured while reading !!!" << ec << std::endl;
		}
	});
}