#include <iostream>
#include <string>

#include "crypto.hh"
#include "parser.hh"

static constexpr uint16_t SERVER_DEFAULT_PORT = 9000;

// Parse an encrypted WebRTC UDP packet
// Encrypted format: nonce (24 bytes) | ciphertext
// Plaintext format: seqno (4 bytes) | data
std::optional<std::pair<uint32_t, std::string>> webrtc_parse( std::string_view payload )
{
  // Payload does not not even include ciphertext, probably invalid
  if ( payload.length() <= NONCE_LEN ) {
    return {};
  }

  // Attempt to decrypt payload
  std::string_view nonce = std::string_view( payload ).substr( 0, NONCE_LEN );
  std::string_view ciphertext = std::string_view( payload ).substr( NONCE_LEN );
  std::optional<std::string> plaintext = decrypt( nonce, ciphertext );
  if ( !plaintext.has_value() ) {
    std::cerr << "Decryption failed!" << std::endl;
    return {};
  }

  // Parse WebRTC application data
  std::string data = plaintext.value();
  uint32_t seqno = str_to_uint<uint32_t>( data );
  data.erase( 0, sizeof( seqno ) );

  return { { seqno, data } };
}

std::string webrtc_serialize( uint32_t seqno, std::string_view data )
{
  std::string plaintext = uint_to_str( seqno ) + std::string( data );
  auto [nonce, ciphertext] = encrypt( plaintext );
  return nonce + ciphertext;
}