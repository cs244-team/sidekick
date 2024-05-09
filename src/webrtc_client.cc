#include "cli11.hh"
#include <list>
#include <string>
#include <optional>
#include <string>
#include <mutex>
#include <chrono>
#include <sys/socket.h> 
#include <cstring> 
#include <iostream> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <vector>
#include <fstream>
#include <map>
#include <condition_variable>
#include "ipv4_datagram.hh"
#include "address.hh"
#include "audio_buffer.hh"
#include "jitter_buffer.hh"
#include "socket.hh"
#include "webrtc_protocol.hh"

//using namespace std;

using namespace std::chrono;


class WebRTCClient{

private:

  UDPSocket clientSocket {}; 
  uint16_t client_port {};
  UDPSocket quackSocket {}; 
  uint16_t quack_port{};

  Address serverAddress;

  uint32_t seqno {};


  unordered_map<uint32_t, std::string> outstanding_dgrams {};


  AudioBuffer buffer {};


  void retransmit(string payload, uint32_t seqno){

    std::cerr << "Retransmitting packet for seqno: " << seqno << std::endl;

    clientSocket.sendto( payload, serverAddress );
  }





public:
  WebRTCClient(uint16_t client_port, uint16_t quack_port, AudioBuffer buf, string ip_addr) : client_port(client_port), 
  quack_port(quack_port), seqno(0), buffer(buf), serverAddress(Address(ip_addr, SERVER_DEFAULT_PORT))
  {
    clientSocket.bind( Address("0.0.0.0", client_port) ); 
    quackSocket.bind( Address("0.0.0.0", quack_port) );
  }

  // Listening thread
  void receive_NACK(){
    while(true){
      std::string payload;
      serverAddress = clientSocket.recvfrom(payload);
      

      // Try to parse encrypted WebRTC data
      auto parse_result = webrtc_parse( payload );
      if ( !parse_result.has_value() ) {
        continue;
      }


      auto [seqno, data] = parse_result.value();

      auto packet = outstanding_dgrams[seqno];
      retransmit(packet, seqno);
    }
  }

  void send_packet(){
    while(!buffer.is_empty()){
      std::string data = buffer.pop(); 

      auto [nonce, ct] = encrypt(uint_to_str(seqno) + data);
      outstanding_dgrams[seqno] = nonce + ct;

      clientSocket.sendto( nonce + ct, serverAddress );
      seqno++;
    }
  }

};

int main( int argc, char* argv[] )
{

  CLI::App app;

  uint16_t client_port = CLIENT_PROTOCOL_DEFAULT_PORT;
  uint16_t quack_port = CLIENT_QUACK_DEFAULT_PORT;

  std::string server_ip = "127.0.0.1";

  //Address server_address = Address( "127.0.0.1", SERVER_DEFAULT_PORT ); 

  app.add_option( "-q,--quack-port", quack_port, "Port to send from client to proxy" )->capture_default_str();
  app.add_option( "-c,--client-port", client_port, "Port to send from client to server" )->capture_default_str();
  app.add_option( "-s,--server-address", server_ip, "IP address of server" )->capture_default_str(); 

  AudioBuffer buffer = {}; // FILL IN AUDIO BUFFER
  

  // Test buffer
  string test = "abcdefghijklm";
  for(int i = 0; i < 10; i++){
    buffer.add_sample(std::to_string(i));
  }

  CLI11_PARSE( app, argc, argv );

  crypto_init();


  WebRTCClient client(client_port, quack_port, buffer, server_ip);

  std::thread nack_thread ( [&] { client.receive_NACK(); } );
  std::thread send_thread ( [&] { client.send_packet(); } );

  nack_thread.join();
  send_thread.join();

  return EXIT_SUCCESS;
}