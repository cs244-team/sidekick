#include <array>
#include <cassert>
#include <iostream>
#include <vector>

#include "crypto.hh"
#include "quack.hh"

int main()
{
  // // Zero packet id doesn't work as expected, which is fine
  // // x^2 - 1x + 0
  // // std::vector<ModInt> pkt_ids = { 0, 1 };

  // std::vector<ModInt> pkt_ids = { 4294967291 - 2, 4294967291 - 1, 4294967291 + 1, 4294967291 + 2, 3, 4, 5 };
  // PowerSums client_power_sums( 8 );
  // PowerSums proxy_power_sums( 8 );

  // for ( size_t i = 0; i < pkt_ids.size(); i++ ) {
  //   client_power_sums.add( pkt_ids[i] );
  // }

  // proxy_power_sums.add( 4294967291 - 2 );
  // proxy_power_sums.add( 4294967291 + 1 );
  // proxy_power_sums.add( 4294967291 + 2 );

  // PowerSums diff = client_power_sums.difference( proxy_power_sums );
  // Polynomial poly( diff );

  // std::cout << "Client sums: " << client_power_sums << std::endl;
  // std::cout << "Proxy sums: " << proxy_power_sums << std::endl;
  // std::cout << "Difference: " << diff << std::endl;
  // std::cout << "Polynomial: " << poly << std::endl;

  // for ( auto id : pkt_ids ) {
  //   if ( poly.eval( id ) == 0 ) {
  //     std::cout << "Proxy did not receive pkt id: " << id << std::endl;
  //   }
  // }

  crypto_init();

  std::string message = "my very secret message!";
  auto [nonce, ct] = encrypt( message );
  auto pt = decrypt( nonce, ct );

  if ( pt.has_value() ) {
    std::cout << pt.value() << std::endl;
  } else {
    std::cout << "Could not decrypt ciphertext" << std::endl;
  }

  ct[0] ^= 0xF1; // Tamper with ciphertext
  pt = decrypt( nonce, ct );

  if ( pt.has_value() ) {
    std::cout << pt.value() << std::endl;
  } else {
    std::cout << "Could not decrypt ciphertext" << std::endl;
  }

  return 0;
}