#include <iostream>

#include "cli11.hh"
#include "sidekick_proxy.hh"

PacketSniffer::PacketSniffer( const std::string& interface, const std::string& filter )
{
  interface_ = interface;
  datagrams_ = std::make_shared<conqueue<IPv4Datagram>>();

  pcap_errbuf_.resize( PCAP_ERRBUF_SIZE );
  pcap_handle_ = pcap_open_live( interface.c_str(), BUFSIZ, PCAP_PROMISC, PCAP_TIMEOUT, pcap_errbuf_.data() );
  if ( pcap_handle_ == NULL ) {
    throw std::runtime_error( "pcap_open_live() failed: " + pcap_errbuf_ );
  }

  // Only capture incoming packets
  if ( pcap_setdirection( pcap_handle_, PCAP_D_IN ) < 0 ) {
    throw std::runtime_error( "pcap_setdirection() failed" );
  }

  // Set filtering rules
  struct bpf_program bpf;
  if ( pcap_compile( pcap_handle_, &bpf, filter.data(), PCAP_OPTIMIZE, 0 ) < 0 ) {
    throw std::runtime_error( "pcap_compile() failed" );
  }

  // Apply filter
  if ( pcap_setfilter( pcap_handle_, &bpf ) == -1 ) {
    throw std::runtime_error( "pcap_setfilter() failed" );
  }
}

void PacketSniffer::run()
{
  std::cerr << "PacketSniffer started, sniffing on interface " << interface_ << std::endl;
  if ( pcap_loop( pcap_handle_, 0, packet_handler, reinterpret_cast<u_char*>( this ) ) < 0 ) {
    throw std::runtime_error( "pcap_loop() failed" );
  }
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
  _this->datagrams_->push( datagram );
}

void SidekickSender::run()
{
  std::cerr << "SidekickSender started" << std::endl;

  // Pull datagrams off the sniffer's queue
  while ( 1 ) {
    IPv4Datagram datagram = datagrams_->pop();
    handle_datagram( datagram );
  }
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
  if ( quacks_.find( src_address ) == quacks_.end() ) {
    quacks_.insert( { src_address, { 0, 0, missing_packet_threshold_ } } );
  }

  // Update quacking state for this sender
  auto& quack = quacks_[src_address];
  quack.num_received++;
  quack.last_received_id = packet_id;
  quack.power_sums.add( packet_id );

  // Send quack to sidekick receiver with the current state
  if ( quack.num_received % quacking_packet_interval_ == 0 ) {
    Address dest( inet_ntoa( { htobe32( src_address ) } ), QUACK_LISTEN_PORT );

    std::cerr << "Sending quack to: " << dest.ip() << ":" << dest.port() << "\n"
              << "num_received: " << quack.num_received << "\n"
              << "last_received_id: " << quack.last_received_id << "\n"
              << "power_sums: " << quack.power_sums << "\n"
              << std::endl;

    auto serialized_quack = serialize( quack );
    std::string payload = std::accumulate( serialized_quack.begin(), serialized_quack.end(), std::string {} );
    quacking_socket_.sendto( payload, dest );
  }
}

int main( int argc, char* argv[] )
{
  CLI::App app;

  std::string interface = "enp0s1";
  std::string pcap_filter = PacketSniffer::DEFAULT_FILTER;
  size_t quacking_interval = 2;
  size_t missing_packet_threshold = 8;

  app.add_option( "-i,--interface", interface, "Interface to sniff packets on" )->capture_default_str();
  app.add_option( "-f,--filter", pcap_filter, "Packet sniffing filter" )->capture_default_str();
  app.add_option( "-q,--quack", quacking_interval, "Send quACKs every q packets" )->capture_default_str();
  app.add_option( "-t,--threshold", missing_packet_threshold, "Missing packet threshold" )->capture_default_str();

  CLI11_PARSE( app, argc, argv );

  PacketSniffer sniffer( interface, pcap_filter );
  SidekickSender sidekick( quacking_interval, missing_packet_threshold, sniffer.datagrams() );

  std::thread sidekick_thread( [&] { sidekick.run(); } );
  std::thread sniffer_thread( [&] { sniffer.run(); } );

  sidekick_thread.join();
  sniffer_thread.join();

  return EXIT_SUCCESS;
}