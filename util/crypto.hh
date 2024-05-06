#pragma once

#include <optional>

#include <sodium.h>

constexpr size_t TAG_LEN = crypto_secretbox_MACBYTES;
constexpr size_t NONCE_LEN = crypto_secretbox_NONCEBYTES;

// Fixed encryption key
static unsigned char key[]
  = { 0xf2, 0x5c, 0xf1, 0x3d, 0xc1, 0x4b, 0x20, 0xd8, 0x13, 0xfa, 0xa3, 0x91, 0xbc, 0x5e, 0xbc, 0x99,
      0x17, 0x79, 0xd3, 0x28, 0x7d, 0x9b, 0x95, 0x46, 0xa7, 0x42, 0x35, 0x90, 0xd5, 0x86, 0x04, 0x25 };

// Has crypto_init has been called?
static bool initialized = false;

// Must be called before any other crypto function
void crypto_init()
{
  if ( sodium_init() < 0 ) {
    throw std::runtime_error( "libsodium could not be initialized" );
  }

  initialized = true;
}

// Authenticated encryption of plaintext with XSalsa20 cipher and Poly1305 MAC; returns nonce | ciphertext
std::pair<std::string, std::string> encrypt( std::string_view plaintext )
{
  if ( !initialized ) {
    throw std::runtime_error( "libsodium has not been initialized with crypto_init()" );
  }

  std::string nonce;
  nonce.resize( NONCE_LEN );
  randombytes_buf( nonce.data(), NONCE_LEN );

  std::string ciphertext;
  ciphertext.resize( plaintext.length() + TAG_LEN );

  unsigned char* ct_buf = reinterpret_cast<unsigned char*>( ciphertext.data() );
  const unsigned char* pt_buf = reinterpret_cast<const unsigned char*>( plaintext.data() );
  const unsigned char* nonce_buf = reinterpret_cast<const unsigned char*>( nonce.data() );
  crypto_secretbox_easy( ct_buf, pt_buf, plaintext.length(), nonce_buf, key );

  return { nonce, ciphertext };
}

std::optional<std::string> decrypt( std::string_view nonce, std::string_view ciphertext )
{
  if ( !initialized ) {
    throw std::runtime_error( "libsodium has not been initialized with crypto_init()" );
  }

  // Not enough information (most likely an invalid ciphertext)
  size_t ct_len = ciphertext.length();
  if ( ct_len < TAG_LEN ) {
    return {};
  }

  std::string plaintext;
  plaintext.resize( ct_len - TAG_LEN );

  unsigned char* pt_buf = reinterpret_cast<unsigned char*>( plaintext.data() );
  const unsigned char* ct_buf = reinterpret_cast<const unsigned char*>( ciphertext.data() );
  const unsigned char* nonce_buf = reinterpret_cast<const unsigned char*>( nonce.data() );
  if ( crypto_secretbox_open_easy( pt_buf, ct_buf, ct_len, nonce_buf, key ) != 0 ) {
    return {};
  }

  return plaintext;
}