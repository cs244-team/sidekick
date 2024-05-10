#include "address.hh"
#include "audio_buffer.hh"
#include "cli11.hh"
#include "ipv4_datagram.hh"
#include "jitter_buffer.hh"
#include "parser.hh"
#include "sidekick_protocol.hh"
#include "socket.hh"
#include "webrtc_protocol.hh"
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// using namespace std;

using namespace std::chrono;

class WebRTCClient
{

private:
  // Send outgoing audio stream
  UDPSocket client_socket_ {};
  uint16_t client_port_ {};

  // Sidekick receiver (TODO (Hari): this should not be here, extract to SidekickReceiver)
  UDPSocket quack_socket_ {};
  uint16_t quack_port_ {};

  // The "peer" we are sending data to
  Address webrtc_server_address_;

  // Next sequence number to dish out
  uint32_t next_seqno_ {};

  unordered_map<uint32_t, std::string> outstanding_dgrams {};

  AudioBuffer buffer {};

  void retransmit( string payload, uint32_t seqno )
  {
    std::cerr << "Retransmitting packet for seqno: " << seqno << std::endl;
    client_socket_.sendto( payload, webrtc_server_address_ );
  }

public:
  WebRTCClient( uint16_t client_port, uint16_t quack_port, AudioBuffer buffer, Address server_address )
    : client_port_( client_port ), quack_port_( quack_port ), webrtc_server_address_( server_address )
  {
    client_socket_.bind( Address( "0.0.0.0", client_port ) );
    quack_socket_.bind( Address( "0.0.0.0", quack_port ) );
  }

  // Listening thread
  void receive_NACK()
  {
    while ( true ) {
      std::string payload;
      webrtc_server_address_ = client_socket_.recvfrom( payload );

      // Try to parse encrypted WebRTC data
      auto parse_result = webrtc_parse( payload );
      if ( !parse_result.has_value() ) {
        continue;
      }

      auto [seqno, data] = parse_result.value();

      auto packet = outstanding_dgrams[seqno];
      retransmit( packet, seqno );
    }
  }

  void send_packet()
  {
    while ( !buffer.is_empty() ) {
      std::string data = buffer.pop();

      auto [nonce, ct] = encrypt( uint_to_str( next_seqno_ ) + data );
      outstanding_dgrams[next_seqno_] = nonce + ct;

      client_socket_.sendto( nonce + ct, webrtc_server_address_ );
      next_seqno_++;
    }
  }
};

int main( int argc, char* argv[] )
{
  CLI::App app;

  // Server details
  std::string server_ip = "0.0.0.0";
  uint16_t server_port = SERVER_DEFAULT_PORT;

  // Client sending port
  uint16_t client_port = CLIENT_DEFAULT_PORT;

  // SidekickReceiver port
  uint16_t quack_port = QUACK_LISTEN_PORT;

  app.add_option( "-i,--server-ip", server_ip, "IP address of server" )->capture_default_str();
  app.add_option( "-p,--server-port", server_port, "Server port to send audio data to" )->capture_default_str();
  app.add_option( "-c,--client-port", client_port, "Port to send audio data from" )->capture_default_str();
  app.add_option( "-q,--quack-port", quack_port, "Port to listen for quacks on" )->capture_default_str();

  AudioBuffer buffer = {}; // FILL IN AUDIO BUFFER

  // Test buffer
  string test = "abcdefghijklm";
  for ( int i = 0; i < 10; i++ ) {
    buffer.add_sample( std::to_string( i ) );
  }

  CLI11_PARSE( app, argc, argv );

  crypto_init();

  WebRTCClient client( client_port, quack_port, buffer, Address( server_ip, server_port ) );

  std::thread nack_thread( [&] { client.receive_NACK(); } );
  std::thread send_thread( [&] { client.send_packet(); } );

  nack_thread.join();
  send_thread.join();

  return EXIT_SUCCESS;
}