#include <iostream>

#include "sidekick_proxy.hh"

PacketSniffer::PacketSniffer( const std::string& interface )
{
  _datagrams = std::make_shared<conqueue<IPv4Datagram>>();
  _pcap_errbuf.resize( PCAP_ERRBUF_SIZE );

  _pcap_handler = pcap_open_live( interface.c_str(), BUFSIZ, PCAP_PROMISC, PCAP_TIMEOUT, _pcap_errbuf.data() );
  if ( _pcap_handler == NULL ) {
    throw std::runtime_error( "pcap_open_live() failed: " + _pcap_errbuf );
  }

  // Only capture incoming packets
  if ( pcap_setdirection( _pcap_handler, PCAP_D_IN ) < 0 ) {
    throw std::runtime_error( "pcap_setdirection() failed" );
  }

  // Set filtering rules
  struct bpf_program bpf;
  if ( pcap_compile( _pcap_handler, &bpf, PCAP_FILTER, PCAP_OPTIMIZE, 0 ) < 0 ) {
    throw std::runtime_error( "pcap_compile() failed" );
  }

  // Apply filter
  if ( pcap_setfilter( _pcap_handler, &bpf ) == -1 ) {
    throw std::runtime_error( "pcap_setfilter() failed" );
  }
}

std::thread PacketSniffer::run()
{
  return std::thread( [&] {
    if ( pcap_loop( _pcap_handler, 0, packet_handler, reinterpret_cast<u_char*>( this ) ) < 0 ) {
      throw std::runtime_error( "pcap_loop() failed" );
    }
  } );
}

void PacketSniffer::packet_handler( u_char* user, const struct pcap_pkthdr* pkthdr, const u_char* packet )
{
  if ( pkthdr->caplen < ETH_HDR_LEN + IP_HDR_LEN ) {
    return;
  }

  IPv4Datagram datagram;
  std::string data( reinterpret_cast<const char*>( packet ) + ETH_HDR_LEN, pkthdr->caplen - ETH_HDR_LEN );
  if ( !parse( datagram, { data } ) ) {
    std::cerr << "Unable to parse IPv4Datagram" << std::endl;
    return;
  }

  PacketSniffer* _this = reinterpret_cast<PacketSniffer*>( user );
  _this->_datagrams->push( datagram );
}

std::thread SidekickSender::run()
{
  return std::thread( [&] {
    // Pull datagrams off the sniffer's queue
    while ( 1 ) {
      IPv4Datagram datagram = _datagrams->pop();
      handle_datagram( datagram );
    }
  } );
}

void SidekickSender::handle_datagram( IPv4Datagram& datagram )
{
  // We only support UDP protocols for this replication study
  if ( datagram.header.proto != IPPROTO_UDP ) {
    std::cerr << "Received datagram: " << datagram.header.to_string() << std::endl;
    return;
  }

  // Retrieve packet id from specific offset in UDP payload as determined by the sidekick protocol
  std::string ip_payload = std::accumulate( datagram.payload.begin(), datagram.payload.end(), std::string {} );
  auto packet_id = get_packet_id( std::string_view( ip_payload ).substr( UDP_HDR_LEN ) );
  if ( packet_id.has_value() ) {
    update_quack( datagram.header.src, packet_id.value() );
  }
}

void SidekickSender::update_quack( IPv4Address src_address, uint32_t packet_id )
{
  if ( _quacks.find( src_address ) == _quacks.end() ) {
    _quacks.insert( { src_address, { 0, 0, _missing_packet_threshold } } );
  }

  // Update quacking state for this sender
  auto& quack = _quacks[src_address];
  quack.num_received++;
  quack.last_received_id = packet_id;
  quack.power_sums.add( packet_id );

  // Send quack to sidekick receiver with the current state
  if ( quack.num_received % _quacking_packet_interval == 0 ) {
    Address dest( inet_ntoa( { htobe32( src_address ) } ), QUACK_LISTEN_PORT );

    std::cerr << "Sending quack to: " << dest.ip() << "\n"
              << "num_received: " << quack.num_received << "\n"
              << "last_received_id: " << quack.last_received_id << "\n"
              << "power_sums: " << quack.power_sums << "\n"
              << std::endl;

    auto serialized_quack = serialize( quack );
    std::string payload = std::accumulate( serialized_quack.begin(), serialized_quack.end(), std::string {} );
    _quacking_socket.sendto( payload, dest );
  }
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    return EXIT_FAILURE;
  }

  auto args = std::span( argv, argc );

  if ( argc != 4 ) {
    std::cerr << "Usage: " << args[0] << " <interface> <quacking_interval> <missing_packet_threshold>" << std::endl;
    return EXIT_FAILURE;
  }

  // Example: ./sidekick_proxy enp0s1 2 8
  std::string interface { args[1] };
  size_t quacking_interval = strtol( args[2], nullptr, 0 );
  size_t missing_packet_threshold = strtol( args[3], nullptr, 0 );

  PacketSniffer sniffer( interface );
  SidekickSender sidekick( quacking_interval, missing_packet_threshold, sniffer.datagrams() );
  std::thread sidekick_thread = sidekick.run();
  std::thread sniffer_thread = sniffer.run();
  sidekick_thread.join();
  sniffer_thread.join();

  return EXIT_SUCCESS;
}