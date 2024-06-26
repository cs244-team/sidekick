#include <fstream>
#include <string>
#include <thread>

#include "cli11.hh"
#include "jitter_buffer.hh"
#include "socket.hh"
#include "webrtc_protocol.hh"

using namespace std::chrono;

class WebRTCServer
{
private:
  // De-jitter buffer for in-order audio playback
  JitterBuffer buffer_ {};

  // Receive incoming audio stream
  UDPSocket socket_ {};
  uint16_t port_ {};

  // Expected RTT in milliseconds
  uint64_t rtt_;

public:
  WebRTCServer( uint16_t port, uint64_t rtt ) : port_( port ), rtt_( rtt )
  {
    socket_.bind( Address( "0.0.0.0", port ) );
  }

  void listen( uint64_t num_expected_seqnos )
  {
    std::cerr << "WebRTCServer started, listening on port " << port_ << std::endl;

    while ( 1 ) {
      std::string payload;
      Address client_address = socket_.recvfrom( payload );

      // Try to parse encrypted WebRTC data
      auto parse_result = webrtc_parse( payload );
      if ( !parse_result.has_value() ) {
        continue;
      }

      auto [seqno, data] = parse_result.value();
      std::cerr << "Received data from: " << client_address.ip() << ":" << client_address.port()
                << ", seqno: " << seqno << ", length: " << data.length() << std::endl;

      // Insert into jitter buffer
      buffer_.push( seqno, data );

      // Check for any missing seqnos which need NACKs sent
      for ( auto& missing_seqno : buffer_.missing_seqnos() ) {
        time_point_t now = high_resolution_clock::now();
        time_point_t last_nack = missing_seqno.second;

        if ( rtt_ < duration_cast<milliseconds>( now - last_nack ).count() ) {
          std::cerr << "Sending NACK for seqno: " << missing_seqno.first << std::endl;

          auto [nonce, ct] = encrypt( uint_to_str( missing_seqno.first ) );
          socket_.sendto( nonce + ct, client_address );

          // Update this seqno's last NACK'ed time
          missing_seqno.second = now;
        }
      }

      if ( buffer_.received_packets().size() == num_expected_seqnos ) {
        std::cerr << "WebRTCServer received every sequence number, listener is exiting..." << std::endl;
        dump_buffer_statistics();
        break;
      }
    }
  }

  // Play back data in-order (just empties the previously played data)
  void drain()
  {
    while ( 1 ) {
      buffer_.pop();
    }
  }

  void dump_buffer_statistics()
  {
    std::ofstream f;
    f.open( "jitter_buffer_stats.csv" );
    f << "seqno,latency_ms\n";
    for ( auto& [seqno, packet] : buffer_.received_packets() ) {
      f << seqno << "," << duration_cast<milliseconds>( packet.playable_at.value() - packet.received_at ).count() << "\n";
    }
    f.close();
  }
};

int main( int argc, char* argv[] )
{
  CLI::App app;

  uint64_t rtt = 150;
  uint16_t port = SERVER_DEFAULT_PORT;

  uint64_t audio_send_frequency = 20; // Send a sample every 20 milliseconds
  uint64_t audio_duration = 20;      // 20 seconds

  app.add_option( "-r,--rtt", rtt, "Estimated RTT between client and server (ms)" )->capture_default_str();
  app.add_option( "-p,--port", port, "Port to listen on" )->capture_default_str();
  app.add_option( "-f,--frequency", audio_send_frequency, "How often a packet the client sends server a packet in milliseconds" )->capture_default_str();
  app.add_option( "-d,--duration", audio_duration, "The length of the audio stream in seconds" )->capture_default_str();

  CLI11_PARSE( app, argc, argv );

  // Initialize crypto library
  crypto_init();

  WebRTCServer server( port, rtt );

  uint64_t num_seqnos = ( 1000 / audio_send_frequency ) * audio_duration;
  std::thread listen_thread( [&] { server.listen( num_seqnos ); } );
  std::thread play_thread( [&] { server.drain(); } );

  listen_thread.join();
  play_thread.join();

  return EXIT_SUCCESS;
}