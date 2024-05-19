#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "address.hh"
#include "audio_buffer.hh"
#include "cli11.hh"
#include "conqueue.hh"
#include "crypto.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include "quack.hh"
#include "sidekick_protocol.hh"
#include "socket.hh"
#include "webrtc_protocol.hh"

class WebRTCClient
{
private:
  // Send outgoing audio stream
  UDPSocket client_socket_ {};
  uint16_t client_port_ {};

  // The "peer" we are sending data to
  Address webrtc_server_address_;

  // Next sequence number to dish out
  uint32_t next_seqno_ {};

  // Input buffer to read data from
  AudioBuffer& input_buffer_;
  std::mutex buffer_mutex {};
  std::condition_variable buffer_cv {};

  // How often to send a packet in milliseconds
  uint64_t send_frequency_;

  // Sidekick receiver components
  UDPSocket quack_socket_ {};
  uint16_t quack_port_ {};
  size_t missing_packet_threshold_ {};

  // Mapping between sequence numbers and encrypted packets (TODO: maybe clear these out after X seconds?)
  std::unordered_map<uint32_t, std::string> sent_data_ {};

  // Mapping between opaque quack (packet) identifiers and seqnos
  std::unordered_map<uint32_t, uint32_t> packet_ids_to_seqnos_ {};

  // In-order packet ids that have been (re-)transmitted
  std::vector<uint32_t> sent_packet_ids_ {};

  // Protects sent_data_, packet_ids_to_seqnos_, sent_packet_ids_ (the above three fields)
  std::mutex receiver_lock_ {};

public:
  WebRTCClient( uint16_t client_port,
                uint16_t quack_port,
                Address server_address,
                AudioBuffer& buffer,
                uint64_t send_frequency,
                size_t missing_packet_threshold = 8 )
    : client_port_( client_port )
    , quack_port_( quack_port )
    , webrtc_server_address_( server_address )
    , input_buffer_( buffer )
    , send_frequency_( send_frequency )
    , missing_packet_threshold_( missing_packet_threshold )
  {
    client_socket_.bind( Address( "0.0.0.0", client_port ) );
    quack_socket_.bind( Address( "0.0.0.0", quack_port ) );
  }

  // Retransmit a packet based on its sequence number (caller must hold receiver_lock_)
  void retransmit( uint32_t seqno, uint32_t packet_id )
  {
    sent_packet_ids_.push_back( packet_id );
    client_socket_.sendto( sent_data_[seqno], webrtc_server_address_ );
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

      uint32_t seqno_val = str_to_uint<uint32_t>( seqno.value() );

      {
        std::unique_lock lk( receiver_lock_ );

        // Get the packet id that corresponds to this sequence number
        std::optional<uint32_t> packet_id = get_packet_id( sent_data_[seqno_val] );
        std::cerr << "Retransmitting packet for seqno based on NACK: " << seqno_val << std::endl;
        retransmit( seqno_val, packet_id.value() );
      }
    }
  }

  void send_packets()
  {
    std::cerr << "WebRTCClient starting to send audio from port " << client_port_ << " to "
              << webrtc_server_address_.ip() << ":" << webrtc_server_address_.port() << std::endl;

    // The client sends a numbered packet containing 240 bytes of data every 20 milliseconds
    while ( true ) {
      std::this_thread::sleep_for( std::chrono::milliseconds( send_frequency_ ) );

      std::string data;
      {
        std::unique_lock lk( buffer_mutex ); // Assume you have a std::mutex buffer_mutex;
        buffer_cv.wait( lk, [&] { return !input_buffer_.is_empty(); } ); // buffer_cv is std::condition_variable
        data = input_buffer_.pop();
      }

      std::string payload = webrtc_serialize( next_seqno_, data );
      std::optional<uint32_t> packet_id = get_packet_id( payload );
      if ( !packet_id.has_value() ) {
        std::cerr << "Audio payload doesn't have enough data to obtain packet identifier" << std::endl;
        continue;
      }

      {
        std::unique_lock lk( receiver_lock_ );
        sent_data_[next_seqno_] = payload;                      // Keep track of payload for future retransmission
        packet_ids_to_seqnos_[packet_id.value()] = next_seqno_; // For Sidekick-mediated retransmission
        sent_packet_ids_.push_back( packet_id.value() );        // Add this packet id to the in-order ids sent
      }

      next_seqno_++;
      client_socket_.sendto( payload, webrtc_server_address_ );
    }
  }

  void receive_quacks()
  {
    std::cerr << "SidekickReceiver started" << std::endl;

    PowerSums running_sums( missing_packet_threshold_ );
    size_t next_unquacked_idx = 0;
    uint32_t num_missing = 0;

    while ( true ) {
      std::string payload;
      Address proxy_address = quack_socket_.recvfrom( payload );

      Quack received_quack;
      if ( !parse( received_quack, { payload } ) ) {
        std::cerr << "Unable to parse quack" << std::endl;
        continue;
      }

      // Calculate power sums from sender's side (set of all sent packets)
      size_t first_quacked_idx = next_unquacked_idx;
      {
        std::unique_lock lk( receiver_lock_ );
        for ( size_t i = next_unquacked_idx; i < sent_packet_ids_.size(); i++ ) {
          running_sums.add( sent_packet_ids_[i] );
          if ( sent_packet_ids_[i] == received_quack.last_received_id ) {
            next_unquacked_idx = i + 1;
            break;
          }
        }
      }

      // // Could be used to detect when missing more packets that `missing_packet_threshold`, but since we
      // // do not reset Sidekick, we can ignore this for now.
      // uint32_t prev_num_missing = num_missing;

      // Derive polynomial with coefficients from difference of power sums, and find roots (missing packets)
      Polynomial diff_poly = Polynomial( running_sums.difference( received_quack.power_sums ) );
      {
        std::unique_lock lk( receiver_lock_ );
        for ( size_t i = first_quacked_idx; i < next_unquacked_idx; i++ ) {
          uint32_t packet_id = sent_packet_ids_[i];
          if ( diff_poly.eval( packet_id ) == 0 ) {
            std::cerr << "Retransmitting missing seqno based on quACK: " << packet_ids_to_seqnos_[packet_id]
                      << std::endl;
            retransmit( packet_ids_to_seqnos_[packet_id], packet_id );
            running_sums.remove( packet_id );
            num_missing++;
          }
        }
      }

      std::cerr << "Received quack from: " << proxy_address.ip() << ":" << proxy_address.port() << "\n"
                << "num_received: " << received_quack.num_received << "\n"
                << "last_received_id: " << received_quack.last_received_id << "\n"
                << "power_sums: " << received_quack.power_sums << "\n"
                << "total packets missing: " << num_missing << "\n"
                << std::endl;
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

  // Audio stream details (default is 10s of 240-byte random audio samples sending at 50 samples/s)
  std::string audio_file_path = "";
  uint64_t audio_send_frequency = 20; // Send a sample every 20 milliseconds
  uint64_t audio_duration = 20;       // 20 seconds
  uint64_t audio_sample_size = 240;   // 240 bytes

  app.add_option( "-i,--server-ip", server_ip, "IP address of server" )->capture_default_str();
  app.add_option( "-p,--server-port", server_port, "Server port to send audio data to" )->capture_default_str();
  app.add_option( "-c,--client-port", client_port, "Port to send audio data from" )->capture_default_str();
  app.add_option( "-q,--quack-port", quack_port, "Port to listen for quacks on" )->capture_default_str();
  app.add_option( "-a,--audio-file", audio_file_path, "WAV file to stream, otherwise random data will be used" );
  app.add_option( "-f,--frequency", audio_send_frequency, "How often to send a packet in milliseconds" )
    ->capture_default_str();
  app
    .add_option(
      "-d,--duration", audio_duration, "The length of the audio stream in seconds, if no audio file specified" )
    ->capture_default_str();
  app
    .add_option(
      "-s,--sample-size", audio_sample_size, "The size of each audio sample in bytes, if no audio file specified" )
    ->capture_default_str();

  CLI11_PARSE( app, argc, argv );

  crypto_init();

  AudioBuffer buffer;
  WebRTCClient client( client_port, quack_port, Address( server_ip, server_port ), buffer, audio_send_frequency );

  std::thread audio_thread( [&]() {
    // Load an audio file or read from /dev/urandom
    if ( !audio_file_path.empty() ) {
      std::string pcm_file_path = audio_file_path + ".pcm";
      AudioBuffer::wav_to_pcm( audio_file_path, pcm_file_path );
      buffer.load_samples( pcm_file_path );
    } else {
      buffer.load_random_samples( ( audio_duration * 1000 ) / audio_send_frequency, audio_sample_size );
    }
  } );

  std::thread nack_thread( [&] { client.receive_nacks(); } );
  std::thread send_thread( [&] { client.send_packets(); } );
  std::thread quack_thread( [&] { client.receive_quacks(); } );

  audio_thread.join();
  nack_thread.join();
  send_thread.join();
  quack_thread.join();

  return EXIT_SUCCESS;
}
