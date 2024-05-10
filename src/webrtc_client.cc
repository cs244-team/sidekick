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

using namespace std::chrono;

class WebRTCClient
{
private:
  // Send outgoing audio stream
  UDPSocket client_socket_ {};
  uint16_t client_port_ {};

  // Sidekick receiver (TODO(Hari): might be best to put this in SidekickReceiver)
  UDPSocket quack_socket_ {};
  uint16_t quack_port_ {};

  // The "peer" we are sending data to
  Address webrtc_server_address_;

  // Next sequence number to dish out
  uint32_t next_seqno_ {};

  // Mapping between sequence numbers and encrypted packets (TODO: maybe clear these out after X seconds?)
  std::unordered_map<uint32_t, std::string> sent_data_ {};

  // Input buffer to read data from
  AudioBuffer buffer {};

  // Retransmit a packet based on its sequence number
  void retransmit( uint32_t seqno )
  {
    std::cerr << "Retransmitting packet for seqno: " << seqno << std::endl;
    client_socket_.sendto( sent_data_[seqno], webrtc_server_address_ );
  }

public:
  WebRTCClient( uint16_t client_port, uint16_t quack_port, AudioBuffer buffer, Address server_address )
    : client_port_( client_port ), quack_port_( quack_port ), webrtc_server_address_( server_address )
  {
    client_socket_.bind( Address( "0.0.0.0", client_port ) );
    quack_socket_.bind( Address( "0.0.0.0", quack_port ) );
  }

  // Listening thread
  void receive_nacks()
  {
    std::cerr << "WebRTCClient listening for NACKs on port " << client_port_ << std::endl;

    while ( true ) {
      std::string payload;
      Address server_address = client_socket_.recvfrom( payload );

      // Decrypt sequence number from NACK
      std::string_view nonce = std::string_view( payload ).substr( 0, NONCE_LEN );
      std::string_view ciphertext = std::string_view( payload ).substr( NONCE_LEN );
      std::optional<std::string> seqno = decrypt( nonce, ciphertext );

      if ( !seqno.has_value() ) {
        std::cerr << "Unable to decrypt NACK" << std::endl;
      }

      std::cerr << "Retransmitting packet for seqno based on NACK: " << seqno.value() << std::endl;
      retransmit( str_to_uint<uint32_t>( seqno.value() ) );
    }
  }

  void send_packet()
  {
    while ( !buffer.is_empty() ) {
      std::string data = buffer.pop();

      auto [nonce, ct] = encrypt( uint_to_str( next_seqno_ ) + data );
      sent_data_[next_seqno_] = nonce + ct;

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

  std::thread nack_thread( [&] { client.receive_nacks(); } );
  std::thread send_thread( [&] { client.send_packet(); } );

  nack_thread.join();
  send_thread.join();

  return EXIT_SUCCESS;
}