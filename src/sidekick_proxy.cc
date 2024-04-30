#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <pcap/pcap.h>

#include "address.hh"
#include "conqueue.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include "quack.hh"

class PacketSniffer
{
private:
  static constexpr int PCAP_PROMISC = 1;
  static constexpr int PCAP_TIMEOUT = 1000;
  static constexpr size_t IP_HEADER_LEN = sizeof( struct iphdr );
  static constexpr size_t ETH_HEADER_LEN = sizeof( struct ethhdr );

  // Datagrams that have been filtered and parsed
  std::shared_ptr<conqueue<IPv4Datagram>> _datagrams;

  // libpcap state
  pcap_t* _pcap_handler;
  std::string _pcap_errbuf;

  // Main packet callback function
  static void packet_handler( u_char* user, const struct pcap_pkthdr* pkthdr, const u_char* packet )
  {
    if ( pkthdr->caplen < ETH_HEADER_LEN + IP_HEADER_LEN ) {
      return;
    }

    IPv4Datagram datagram;
    std::string data( reinterpret_cast<const char*>( packet ) + ETH_HEADER_LEN, pkthdr->caplen );
    if ( !parse( datagram, { data } ) ) {
      std::cerr << "Unable to parse IPv4Datagram" << std::endl;
      return;
    }

    PacketSniffer* _this = reinterpret_cast<PacketSniffer*>( user );
    _this->_datagrams->push( datagram );
  }

public:
  PacketSniffer( std::string& interface, std::string& filter )
  {
    _datagrams = std::make_shared<conqueue<IPv4Datagram>>();
    _pcap_errbuf.resize( PCAP_ERRBUF_SIZE );

    _pcap_handler = pcap_open_live( interface.c_str(), BUFSIZ, PCAP_PROMISC, PCAP_TIMEOUT, _pcap_errbuf.data() );
    if ( _pcap_handler == NULL ) {
      throw std::runtime_error( "pcap_open_live() failed: " + _pcap_errbuf );
    }

    // Only capture incoming packets
    // TODO: still has a problem, how do we ignore packets coming back from WebRTC server?
    if ( pcap_setdirection( _pcap_handler, PCAP_D_IN ) < 0 ) {
      throw std::runtime_error( "pcap_setdirection() failed" );
    }

    // Set filtering rules
    struct bpf_program bpf;
    if ( pcap_compile( _pcap_handler, &bpf, filter.c_str(), 1, 0 ) < 0 ) {
      throw std::runtime_error( "pcap_compile() failed" );
    }

    // Apply filter
    if ( pcap_setfilter( _pcap_handler, &bpf ) == -1 ) {
      throw std::runtime_error( "pcap_setfilter() failed" );
    }
  }

  ~PacketSniffer()
  {
    if ( _pcap_handler ) {
      pcap_close( _pcap_handler );
    }
  }

  // Start a new sniffing thread
  std::thread start_capture()
  {
    return std::thread( [&] {
      if ( pcap_loop( _pcap_handler, 0, packet_handler, reinterpret_cast<u_char*>( this ) ) < 0 ) {
        throw std::runtime_error( "pcap_loop() failed" );
      }
    } );
  }

  std::shared_ptr<conqueue<IPv4Datagram>> datagrams() { return _datagrams; }
};

class SidekickSender
{
private:
  std::shared_ptr<conqueue<IPv4Datagram>> _datagrams;
  // std::unordered_map<Address, PowerSums> _quacks {};

public:
  SidekickSender( std::shared_ptr<conqueue<IPv4Datagram>> queue ) : _datagrams( queue ) {};

  std::thread start_quacking()
  {
    return std::thread( [&] {
      // Pull datagrams off the sniffer's queue
      while ( 1 ) {
        IPv4Datagram datagram = _datagrams->pop();
        handle_datagram( datagram );
      }
    } );
  }

  void handle_datagram( IPv4Datagram& datagram )
  {
    std::cout << datagram.header.to_string() << std::endl;
    // TODO: capture "packet IDs" from UDP payload, at a specific offset
    // If the payload is encrypted, then we just take the 4 bytes at a pre-determined offset
    // Otherwise, we need to hash the bytes ourselves.
    // Maybe make no assumptions about underyling protocol and just hash anyways?
  }
};

int main( int argc, char* argv[] )
{
  std::string interface = "enp0s1"; // get from args
  std::string filter = "ip && udp"; // IPv4 and UDP packets

  PacketSniffer sniffer( interface, filter );
  SidekickSender sidekick( sniffer.datagrams() );

  std::thread sidekick_thread = sidekick.start_quacking();
  std::thread sniffer_thread = sniffer.start_capture();

  sidekick_thread.join();
  sniffer_thread.join();

  return 0;
}