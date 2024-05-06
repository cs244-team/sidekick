// Sidekick protocol details
#pragma once

#include <optional>

#include "parser.hh"
#include "quack.hh"

static constexpr uint16_t QUACK_LISTEN_PORT = 8765;

// Four bytes of UDP payload at `QUACK_ID_OFFSET` will be used as the opaque packet id
static constexpr uint16_t QUACK_ID_OFFSET = 8;

struct Quack
{
  uint32_t num_received {};
  uint32_t last_received_id {};
  PowerSums power_sums { 0 };

  void parse( Parser& parser )
  {
    parser.integer( num_received );
    parser.integer( last_received_id );

    uint32_t tmp;
    std::vector<ModInt> sums {};
    while ( !parser.buffer().empty() ) {
      parser.integer( tmp );
      sums.push_back( tmp );
    }

    power_sums = { sums };
  }

  void serialize( Serializer& serializer ) const
  {
    serializer.integer( num_received );
    serializer.integer( last_received_id );
    for ( size_t i = 0; i < power_sums.size(); i++ ) {
      serializer.integer( power_sums[i].value() );
    }
  }
};

// Get an opaque identifier from a UDP datagram at `QUACK_ID_OFFSET`
std::optional<uint32_t> get_packet_id( std::string_view udp_payload )
{
  // Not enough data to grab a packet identifier
  if ( udp_payload.length() < QUACK_ID_OFFSET + sizeof( uint32_t ) ) {
    return {};
  }
  return str_to_uint<uint32_t>( udp_payload.substr( QUACK_ID_OFFSET ) );
}