// Taken from Stanford CS144: https://github.com/cs144/minnow
#pragma once

#include <arpa/inet.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "parser.hh"

class InternetChecksum
{
private:
  uint32_t sum_;
  bool parity_ {};

public:
  explicit InternetChecksum( const uint32_t sum = 0 ) : sum_( sum ) {}
  void add( std::string_view data )
  {
    for ( const uint8_t i : data ) {
      uint16_t val = i;
      if ( not parity_ ) {
        val <<= 8;
      }
      sum_ += val;
      parity_ = !parity_;
    }
  }

  uint16_t value() const
  {
    uint32_t ret = sum_;

    while ( ret > 0xffff ) {
      ret = ( ret >> 16 ) + static_cast<uint16_t>( ret );
    }

    return ~ret;
  }

  void add( const std::vector<std::string>& data )
  {
    for ( const auto& x : data ) {
      add( x );
    }
  }

  void add( const std::vector<std::string_view>& data )
  {
    for ( const auto& x : data ) {
      add( x );
    }
  }
};

// IPv4 Internet datagram header (note: IP options are not supported)
struct IPv4Header
{
  static constexpr size_t LENGTH = 20; // IPv4 header length, not including options

  // IPv4 Header fields
  uint8_t ver = 4;           // IP version
  uint8_t hlen = LENGTH / 4; // header length (multiples of 32 bits)
  uint8_t tos = 0;           // type of service
  uint16_t len = 0;          // total length of packet
  uint16_t id = 0;           // identification number
  bool df = true;            // don't fragment flag
  bool mf = false;           // more fragments flag
  uint16_t offset = 0;       // fragment offset field
  uint8_t ttl = 64;          // time to live field
  uint8_t proto = 0;         // protocol field
  uint16_t cksum = 0;        // checksum field
  uint32_t src = 0;          // src address
  uint32_t dst = 0;          // dst address

  // Length of the payload
  uint16_t payload_length() const { return len - 4 * hlen; }

  // Return a string containing a header in human-readable format
  std::string to_string() const
  {
    std::stringstream ss {};
    ss << std::hex << std::boolalpha << "IPv" << +ver << " len=" << std::dec << +len << " protocol=" << +proto
       << " ttl=" + std::to_string( ttl ) << " src=" << inet_ntoa( { htobe32( src ) } )
       << " dst=" << inet_ntoa( { htobe32( dst ) } );
    return ss.str();
  }

  void compute_checksum()
  {
    cksum = 0;
    Serializer s;
    serialize( s );

    // calculate checksum -- taken over header only
    InternetChecksum check;
    check.add( s.output() );
    cksum = check.value();
  }

  void parse( Parser& parser )
  {
    uint8_t first_byte {};
    parser.integer( first_byte );
    ver = first_byte >> 4;    // version
    hlen = first_byte & 0x0f; // header length
    parser.integer( tos );    // type of service
    parser.integer( len );
    parser.integer( id );

    uint16_t fo_val {};
    parser.integer( fo_val );
    df = static_cast<bool>( fo_val & 0x4000 ); // don't fragment
    mf = static_cast<bool>( fo_val & 0x2000 ); // more fragments
    offset = fo_val & 0x1fff;                  // offset

    parser.integer( ttl );
    parser.integer( proto );
    parser.integer( cksum );
    parser.integer( src );
    parser.integer( dst );

    if ( ver != 4 ) {
      parser.set_error();
    }

    if ( hlen < 5 ) {
      parser.set_error();
    }

    if ( parser.has_error() ) {
      return;
    }

    parser.remove_prefix( static_cast<uint64_t>( hlen ) * 4 - IPv4Header::LENGTH );

    // Verify checksum
    const uint16_t given_cksum = cksum;
    compute_checksum();
    if ( cksum != given_cksum ) {
      parser.set_error();
    }
  }

  void serialize( Serializer& serializer ) const
  {
    // consistency checks
    if ( ver != 4 ) {
      throw std::runtime_error( "wrong IP version" );
    }

    const uint8_t first_byte = ( static_cast<uint32_t>( ver ) << 4 ) | ( hlen & 0xfU );
    serializer.integer( first_byte ); // version and header length
    serializer.integer( tos );
    serializer.integer( len );
    serializer.integer( id );

    const uint16_t fo_val = ( df ? 0x4000U : 0 ) | ( mf ? 0x2000U : 0 ) | ( offset & 0x1fffU );
    serializer.integer( fo_val );

    serializer.integer( ttl );
    serializer.integer( proto );

    serializer.integer( cksum );

    serializer.integer( src );
    serializer.integer( dst );
  }
};

struct IPv4Datagram
{
  IPv4Header header {};
  std::vector<std::string> payload {};

  void parse( Parser& parser )
  {
    header.parse( parser );
    parser.all_remaining( payload );
  }

  void serialize( Serializer& serializer ) const
  {
    header.serialize( serializer );
    for ( const auto& x : payload ) {
      serializer.buffer( x );
    }
  }
};
