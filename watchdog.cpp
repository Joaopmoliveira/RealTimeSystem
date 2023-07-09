#include <asio.hpp>
#include <ctime>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>
#include "message_sizes.h"
#include <array>

constexpr auto maximum_delay_in_milliseconds = std::chrono::milliseconds(5);

void safety_shutdown(){
   std::cout << "terminating with safety stop because something went wrong\n";
}

struct Client{
  asio::high_resolution_timer timer;
  asio::ip::tcp::socket client_socket_;
  asio::ip::tcp::socket sensor_socket_;
  std::array<unsigned char,buffer_size> mega_buffer;
  std::atomic<bool> data_sent = false;
  asio::io_context& context;
  ClientControlLawMessage control_law;
  ClientControlLawMessageHeader control_law_header;
  ClientObservationsMessage observations;

  explicit Client(asio::io_context& in_context,
                  asio::ip::tcp::socket&& in_client_socket,
                  asio::ip::tcp::socket&& in_sensor_socket) : context{in_context} , 
                                                              timer{in_context},
                                                              client_socket_{std::move(in_client_socket)}, 
                                                              sensor_socket_{std::move(in_sensor_socket)}{}

  Client(const Client & copyclient) = delete;

  Client(Client && client) : context{client.context} , 
                             timer{std::move(client.timer)}, 
                             client_socket_{std::move(client.client_socket_)}, 
                             sensor_socket_{std::move(client.sensor_socket_)}, 
                             mega_buffer{std::move(client.mega_buffer)} {
  }

  ~Client(){
    timer.cancel();
    sensor_socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
    client_socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
    safety_shutdown();
  }
};

void do_read_sensors(Client& client);
void do_write_message(Client& client);
void do_read_header(Client& client);
void do_read_body(Client& client);
void do_control(Client& client);

void do_read_sensors(Client& client) {
  client.timer.expires_from_now(maximum_delay_in_milliseconds);
  client.timer.async_wait([&](asio::error_code ec) {
    if(client.data_sent){
      client.data_sent = false;
      do_read_sensors(client);
      return ;
    }
    else{
      client.context.stop();
      return ;
    } 
  });

  asio::async_read( client.sensor_socket_, asio::buffer(client.mega_buffer),asio::transfer_exactly(Measurments::measurments_size),
    [ &client](asio::error_code ec, size_t /*length*/) {
        if (ec || !unpack_observation_message(client.mega_buffer.data(),Measurments::measurments_size,client.observations)) {
          client.context.stop();
          return ;
        } 
        do_write_message(client);
  });
}

void do_write_message(Client& client) {
  size_t message_size =0;
  pack_observation_message(client.observations,client.mega_buffer.data(),message_size);
  asio::async_write( client.client_socket_, asio::buffer(client.mega_buffer), asio::transfer_exactly(Measurments::measurments_size),
    [ &client](asio::error_code ec, size_t /*length*/) {
        if (ec) {
          client.context.stop();
          return ;
        } 
        do_read_header(client);
  });
}

void do_read_header(Client& client) {
  asio::async_read( client.client_socket_, asio::buffer(client.mega_buffer), asio::transfer_exactly(ClientControlLawMessageHeader::client_control_law_header_size),
    [ &client](asio::error_code ec, size_t /*length*/) {
      if (ec || !unpack_control_law_header(client.mega_buffer.data(),ClientControlLawMessageHeader::client_control_law_header_size,client.control_law_header)) {
        client.context.stop();
        return ;
      } 
      do_read_body(client);
  });
}

void do_read_body(Client& client) {
  asio::async_read( client.client_socket_, asio::buffer(client.mega_buffer), asio::transfer_exactly(client.control_law_header.size_of_control_law),
    [ &client](asio::error_code ec, size_t /*length*/) {
      if (ec || !unpack_control_law_message(client.mega_buffer.data(),client.control_law_header.size_of_control_law,client.control_law)) {
        client.context.stop();
        return ;
      } 
      do_control(client);
  });
};

void do_control(Client& client) {
  size_t message_size = 0;
  pack_header_and_control_law_message(client.control_law_header,client.control_law,client.mega_buffer.data(),message_size);
  asio::async_write( client.sensor_socket_, asio::buffer(client.mega_buffer), asio::transfer_exactly(client.control_law_header.size_of_control_law),
    [ &client](asio::error_code ec, size_t /*length*/) {
        if (ec) {
          client.context.stop();
          return ;
        } 
        client.data_sent = true;
  });
};


int main(int argc, char* argv[])
{
  if(argc!=4){
    std::cout << "To call this executable provide 3 arguments \n- ip , e.g. \"localhost\" \n- port , e.g. 30000\n- server of watchdog , e.g. 15000" << std::endl;
    return 1;
  }
  asio::io_context io_context;
  // the sensors are a must for us to connect to, thus we must connect syncronously to them
  asio::ip::tcp::socket sensor_socket(io_context);
  asio::ip::tcp::resolver sensor_resolver(io_context);
  asio::connect(sensor_socket, sensor_resolver.resolve(argv[1], argv[2]));

  // once the connection has been established we create a server that the client can connect himself to us. 
  asio::error_code ec;
  std::string string_port{argv[3]};

  std::size_t pos = 0;
  try
  {
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
    safety_shutdown();
    return 1;
  }

  // As soon as that connection is established we can start our internal timer which will scream
  // as soon as the state machine either fails the connection or the timer expires
  Client watchgod{io_context,std::move(client_socket),std::move(sensor_socket)};
  
  // Here is where we lauch our state machine
  do_read_sensors(watchgod);

  //this is a blocking call
  io_context.run();
  return 0;
};

