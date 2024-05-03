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
#include "protocol.hh"
#include "quack.hh"
#include "socket.hh"

static constexpr size_t ETH_HDR_LEN = sizeof( struct ethhdr );
static constexpr size_t IP_HDR_LEN = sizeof( struct iphdr );
static constexpr size_t UDP_HDR_LEN = sizeof( struct udphdr );

class PacketSniffer
{
private:
  static constexpr int PCAP_PROMISC = 1;
  static constexpr int PCAP_TIMEOUT = -1;
  static constexpr int PCAP_OPTIMIZE = 1;

  // Datagrams that have been filtered and parsed
  std::shared_ptr<conqueue<IPv4Datagram>> _datagrams;

  // libpcap state
  pcap_t* _pcap_handler;
  std::string _pcap_errbuf;

  // Main packet callback function
  static void packet_handler( u_char* user, const struct pcap_pkthdr* pkthdr, const u_char* packet );

public:
  PacketSniffer( std::string& interface, std::string& filter );
  ~PacketSniffer()
  {
    if ( _pcap_handler ) {
      pcap_close( _pcap_handler );
    }
  };

  std::thread start_capture();
  std::shared_ptr<conqueue<IPv4Datagram>> datagrams() { return _datagrams; }
};

class SidekickSender
{
private:
  // Sender configuration
  size_t _quacking_packet_interval {};
  size_t _missing_packet_threshold {};

  // Datagrams captured by sniffer
  std::shared_ptr<conqueue<IPv4Datagram>> _datagrams;

  // quACK state mapped to sender IPv4 addresses
  std::unordered_map<IPv4Address, Quack> _quacks {};

  // Socket to send quACKs from proxy to sidekick receivers
  UDPSocket _quacking_socket {};

public:
  SidekickSender( size_t quacking_packet_interval,
                  size_t missing_packet_threshold,
                  std::shared_ptr<conqueue<IPv4Datagram>> datagrams )
    : _quacking_packet_interval( quacking_packet_interval )
    , _missing_packet_threshold( missing_packet_threshold )
    , _datagrams( datagrams )
  {
    _quacking_socket.bind( Address( "0.0.0.0", 0 ) );
  };

  std::thread start_quacking();
  void handle_datagram( IPv4Datagram& datagram );
  void update_quack( IPv4Address src_address, uint32_t packet_id );
};