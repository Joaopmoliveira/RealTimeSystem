#include <asio.hpp>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>
#include "message_sizes.h"
#include <array>

int main(int argc, char* argv[]){
if(argc!=4){
    std::cout << "To call this executable provide 3 arguments \n- ip , e.g. \"localhost\" \n- port , e.g. 30000\n- server of watchdog , e.g. 15000" << std::endl;
    return 1;
  }

    asio::io_context io_context;
    // once the connection has been established we create a server that the client can connect himself to us. 
    asio::error_code ec;
    std::string string_port{argv[1]};

    std::size_t pos = 0;
    try{
        std::cout << "std::stoi('" << string_port << "'): ";
        const int i{std::stoi(string_port, &pos)};
        std::cout << i << "; pos: " << pos << '\n';
    } catch (std::invalid_argument const& ex){
        std::cout << "std::invalid_argument::what(): " << ex.what() << '\n';
        return 1;
    } catch (std::out_of_range const& ex){
        std::cout << "std::out_of_range::what(): " << ex.what() << '\n';
        const long long ll{std::stoll(string_port, &pos)};
        std::cout << "std::stoll('" << string_port << "'): " << ll << "; pos: " << pos << '\n';
        return 1;
    }

    short port = pos;
    asio::ip::tcp::endpoint endpoit(asio::ip::tcp::v4(), port);
    asio::ip::tcp::acceptor acceptor(io_context);
    asio::ip::tcp::socket client_socket(io_context);
    acceptor.accept(client_socket,endpoit,ec);
    if (ec){
        return 1;
    };

    ClientObservationsMessage observations;
    ClientControlLawMessageHeader header;
    ClientControlLawMessage body;
    std::array<unsigned char,buffer_size> mega_buffer;

    while(true){
        size_t message_size = 0;
        pack_observation_message(observations,mega_buffer.data(),message_size);
        asio::write(client_socket,asio::buffer(mega_buffer),asio::transfer_exactly(message_size),ec);

        //we want to go as fast as possible, no sleeping in here
        asio::read(client_socket,asio::buffer(mega_buffer),asio::transfer_exactly(ClientControlLawMessageHeader::client_control_law_header_size),ec);
        if(ec && !unpack_control_law_header(mega_buffer.data(),ClientControlLawMessageHeader::client_control_law_header_size,header)){
            std::cout << "watchdog stopped the connection\n";
            return 1;
        }
        asio::read(client_socket,asio::buffer(mega_buffer),asio::transfer_exactly(header.size_of_control_law),ec);
        if(ec || !unpack_control_law_message(mega_buffer.data(),header.size_of_control_law,body)){
            std::cout << "watchdog stopped the connection\n";
            return 1;
        }
    };
    return 0;
}