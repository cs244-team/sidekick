#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

#include "conqueue.hh"

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point_t;

class JitterBuffer
{
private:
  struct Packet
  {
    time_point_t received_at {};
    std::optional<time_point_t> playable_at {};
    std::string data {};
  };

  // Mapping between sequence numbers and packet data
  // For simplicity, never remove an element after insertion, just update its `playable_at` time
  std::unordered_map<uint32_t, Packet> received_packets_ {};

  // Mapping between sequence numbers and time of last NACK
  std::unordered_map<uint32_t, time_point_t> missing_seqnos_ {};

  // Received and playable in-order packet data (for audo playback)
  conqueue<std::string_view> playback_queue_ {};

  // Next expected sequence number
  uint32_t next_seqno_ {};

  // The next sequence number that cannot immediately be played in-order from the buffer
  uint32_t next_unplayable_seqno_ {};

public:
  // For latency calculations
  std::unordered_map<uint32_t, Packet> received_packets() const { return received_packets_; }

  // In order to update time of last NACK upon retransmission
  std::unordered_map<uint32_t, time_point_t> missing_packets() { return missing_seqnos_; }

  // Add data to buffer, and check if any data can be immediately played back
  void push( uint32_t seqno, std::string& data )
  {
    if ( received_packets_.find( seqno ) != received_packets_.end() ) {
      std::cerr << "Packet has already been received, seqno: " << seqno << std::endl;
      return;
    }

    // Add missing packets and assume that the caller will want to transmit a NACK for this packet right away,
    // so time of last NACK is left unspecified
    while ( next_seqno_ <= seqno ) {
      missing_seqnos_[next_seqno_++] = {};
    }

    // Add this newly received packet
    auto time = std::chrono::high_resolution_clock::now();
    received_packets_[seqno] = { .received_at = time, .data = std::move( data ) };
    missing_seqnos_.erase( seqno );

    // Set packets' "playable_at" time, but don't block on inserting into outbound queue
    std::vector<std::string_view> playable_data;
    while ( received_packets_.find( next_unplayable_seqno_ ) != received_packets_.end() ) {
      auto& packet = received_packets_[next_unplayable_seqno_++];
      packet.playable_at = std::chrono::high_resolution_clock::now();
      playable_data.push_back( packet.data );
    }

    for ( auto& data : playable_data ) {
      playback_queue_.push( data );
    }
  }

  std::string_view pop() { return playback_queue_.pop(); }

  friend std::ostream& operator<<( std::ostream& stream, const JitterBuffer& obj )
  {
    stream << "Received seqnos: { ";
    for ( auto& packet : obj.received_packets_ ) {
      stream << packet.first << ", ";
    }
    stream << "}, Missing seqnos: { ";
    for ( auto& packet : obj.missing_seqnos_ ) {
      stream << packet.first << ", ";
    }
    stream << "}";

    return stream;
  }
};