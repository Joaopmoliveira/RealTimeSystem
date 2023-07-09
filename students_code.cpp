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
  // the sensors are a must for us to connect to, thus we must connect syncronously to them
  asio::ip::tcp::socket sensor_socket(io_context);
  asio::ip::tcp::resolver sensor_resolver(io_context);
  asio::connect(sensor_socket, sensor_resolver.resolve(argv[1], argv[2]));
  
  ClientObservationsMessage observations;
  ClientControlLawMessageHeader header;
  ClientControlLawMessage body;
  asio::error_code ec;
  std::array<unsigned char,buffer_size> mega_buffer;

  while(true){
    asio::read(sensor_socket,asio::buffer(mega_buffer),asio::transfer_exactly(Measurments::measurments_size),ec);
    if(ec || !unpack_observation_message(mega_buffer.data(),Measurments::measurments_size,observations)){
        std::cout << "watchdog stopped the connection\n";
        return 1;
    }


    // do your control actions!
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    size_t message_size = 0;
    pack_header_and_control_law_message(header,body,mega_buffer.data(),message_size);
    asio::write(sensor_socket,asio::buffer(mega_buffer),asio::transfer_exactly(message_size),ec);
  }
  return 0;
}