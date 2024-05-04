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
  // TODO: could add some more filtering here (e.g., ignore multicast destinations)?
  static constexpr const char* PCAP_FILTER = "ip && udp";
  static constexpr int PCAP_PROMISC = 1;
  static constexpr int PCAP_TIMEOUT = -1;
  static constexpr int PCAP_OPTIMIZE = 1;

  // Datagrams that have been filtered and parsed
  std::shared_ptr<conqueue<IPv4Datagram>> datagrams_;

  // libpcap state
  pcap_t* pcap_handle_;
  std::string pcap_errbuf_;

  // Main packet callback function
  static void packet_handler( u_char* user, const struct pcap_pkthdr* pkthdr, const u_char* packet );

public:
  PacketSniffer( const std::string& interface );
  ~PacketSniffer()
  {
    if ( pcap_handle_ ) {
      pcap_close( pcap_handle_ );
    }
  };

  std::thread run();
  std::shared_ptr<conqueue<IPv4Datagram>> datagrams() { return datagrams_; }
};

class SidekickSender
{
private:
  // Sender configuration
  size_t quacking_packet_interval_;
  size_t missing_packet_threshold_;

  // Datagrams captured by sniffer
  std::shared_ptr<conqueue<IPv4Datagram>> datagrams_;

  // quACK state mapped to sender IPv4 addresses
  std::unordered_map<IPv4Address, Quack> quacks_ {};

  // Socket to send quACKs from proxy to sidekick receivers
  UDPSocket quacking_socket_ {};

public:
  SidekickSender( size_t quacking_packet_interval,
                  size_t missing_packet_threshold,
                  std::shared_ptr<conqueue<IPv4Datagram>> datagrams )
    : quacking_packet_interval_( quacking_packet_interval )
    , missing_packet_threshold_( missing_packet_threshold )
    , datagrams_( datagrams )
  {
    quacking_socket_.bind( Address( "0.0.0.0", 0 ) );
  };

  std::thread run();
  void handle_datagram( IPv4Datagram& datagram );
  void update_quack( IPv4Address src_address, uint32_t packet_id );
};