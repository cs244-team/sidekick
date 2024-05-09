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
#include <vector.h>
#include <map.h>
#include <condition_variable>
#include "ipv4_datagram.hh"
#include "tcp_receiver_message.hh"
#include "address.hh"

using namespace std;


class WebRTCClient{

private:

  struct EncryptedPacket
  {
    string_view nonce;
    string_view ciphertext;
  };

  struct PlaintextPacket
  {
    uint32_t seqno;
    string_view data;
  };

  UDPSocket clientSocket {}; 
  uint16_t protocol_port {};
  UDPSocket quackSocket {}; 
  uint16_t quack_port{};

  Address serverAddress {};

  uint32_t seqno {};


  unordered_map<uint32_t, Packet> outstanding_dgrams {};


  queue<IPv4Datagram> audio_samples {};


  void retransmit(string payload, uint32_t seqno){

    std::cerr << "Retransmitting packet for seqno: " << seqno << std::endl;

    clientSocket.sendto( payload, serverAddress );
  }

  void send_packet(){
    string data = audio_samples.pop(); // FILL OUT AUDIO SAMPLES

    auto [nonce, ct] = encrypt(uint_to_str(seqno) + data);
    outstanding_dgrams[seqno] = nonce + ct;

    clientSocket.sendto( nonce + ct, serverAddress );
    seqno++;
  }

  // Listening thread
  void receive_NACK(){
    string payload;
    serverAddress = serverSocket.recvfrom(payload);

    // Try to parse encrypted WebRTC data
    auto parse_result = webrtc_parse( payload );
    if ( !parse_result.has_value() ) {
      continue;
    }

    auto [seqno, data] = parse_result.value();

    auto packet = outstanding_dgrams[seqno];
    retransmit(packet, seqno);
  }


public:
  WebRTCClient(uint16_t protocol_port, uint16_t quack_port) : protocol_port(protocol_port), 
  quack_port(quack_port), seqno(0)
  {
    clientSocket.bind( Address("0.0.0.0", client_port) ); 
    quackSocket.bind( Address("0.0.0.0", quack_port) );
  }

}

int main( int argc, char* argv[] )
{
  crypto_init();

  uint16_t protocol_port = CLIENT_PROTOCOL_DEFAULT_PORT;
  uint16_t quack_port = CLIENT_QUACK_DEFAULT_PORT;

  WebRTCClient client(protocol_port, quack_port);

  thread nack_thread ( [&] { client.receive_NACK(); } );
  thread send_thread ( [&] { client.send_packet(); } );

  nack_thread.join();
  send_thread.join();

  return EXIT_SUCCESS;
}