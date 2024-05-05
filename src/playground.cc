#include <array>
#include <cassert>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "crypto.hh"
#include "jitter_buffer.hh"
#include "quack.hh"

void arithmetic()
{
  // Zero packet id doesn't work as expected, which is fine
  // x^2 - 1x + 0
  // std::vector<ModInt> pkt_ids = { 0, 1 };

  std::vector<ModInt> pkt_ids = { 4294967291 - 2, 4294967291 - 1, 4294967291 + 1, 4294967291 + 2, 3, 4, 5 };
  PowerSums client_power_sums( 8 );
  PowerSums proxy_power_sums( 8 );

  for ( size_t i = 0; i < pkt_ids.size(); i++ ) {
    client_power_sums.add( pkt_ids[i] );
  }

  proxy_power_sums.add( 4294967291 - 2 );
  proxy_power_sums.add( 4294967291 + 1 );
  proxy_power_sums.add( 4294967291 + 2 );

  PowerSums diff = client_power_sums.difference( proxy_power_sums );
  Polynomial poly( diff );

  std::cout << "Client sums: " << client_power_sums << std::endl;
  std::cout << "Proxy sums: " << proxy_power_sums << std::endl;
  std::cout << "Difference: " << diff << std::endl;
  std::cout << "Polynomial: " << poly << std::endl;

  for ( auto id : pkt_ids ) {
    if ( poly.eval( id ) == 0 ) {
      std::cout << "Proxy did not receive pkt id: " << id << std::endl;
    }
  }
}

void crypto()
{
  crypto_init();

  std::string message = "my very secret message!";
  auto [nonce, ct] = encrypt( message );
  auto pt = decrypt( nonce, ct );

  if ( pt.has_value() ) {
    std::cout << "Decrypted message: " << pt.value() << std::endl;
  } else {
    std::cout << "Could not decrypt ciphertext" << std::endl;
  }

  ct[0] ^= 0xF1; // Tamper with ciphertext
  pt = decrypt( nonce, ct );

  if ( pt.has_value() ) {
    std::cout << "Decrypted message: " << pt.value() << std::endl;
  } else {
    std::cout << "Could not decrypt ciphertext" << std::endl;
  }
}

void push_with_jitter( JitterBuffer& buf, uint32_t seqno )
{
  static std::mt19937_64 eng { std::random_device {}() };
  static std::uniform_int_distribution<> dist { 50, 200 };
  std::this_thread::sleep_for( std::chrono::milliseconds( dist( eng ) ) );
  std::string audio = "<audio data> at seqno " + std::to_string( seqno );
  buf.push( seqno, audio );
}

void jitter_buffer()
{
  JitterBuffer buf;
  push_with_jitter( buf, 3 );
  push_with_jitter( buf, 4 );
  push_with_jitter( buf, 2 );
  push_with_jitter( buf, 1 );
  push_with_jitter( buf, 6 );
  push_with_jitter( buf, 5 );
  push_with_jitter( buf, 0 );

  for ( auto& packet : buf.received_packets() ) {
    if ( !packet.second.playable_at.has_value() ) {
      continue;
    }
    auto received_time = packet.second.received_at;
    auto removed_time = packet.second.playable_at.value();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>( removed_time - received_time );
    std::cout << "seqno: " << packet.first << ", de-jitter latency: " << ms.count() << " (ms)" << std::endl;
  }

  std::cout << "in-order playback:" << std::endl;
  while ( 1 ) {
    std::cout << buf.pop() << std::endl;
  }
}

int main()
{
  // arithmetic();
  // crypto();
  jitter_buffer();

  return EXIT_SUCCESS;
}