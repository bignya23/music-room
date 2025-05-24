#include <boost/asio.hpp>
#include <iostream>

int main() {
    boost::asio::io_context io;
    std::cout << "Boost.Asio works!" << std::endl;
    return 0;
}
