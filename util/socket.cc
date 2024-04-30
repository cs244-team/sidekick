#include "socket.hh"

UDPSocket::UDPSocket()
{
  if ( ( fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
    throw std::runtime_error( "Failed to open UDPSocket" );
  }
}

UDPSocket::~UDPSocket()
{
  close( fd );
}

void UDPSocket::bind( const Address& address )
{
  if ( ::bind( fd, address.raw(), address.size() ) < 0 ) {
    throw std::runtime_error( "bind() failed" );
  }
};

void UDPSocket::sendto( std::string_view buf, const Address& address )
{
  if ( ::sendto( fd, buf.data(), buf.length(), 0, address.raw(), address.size() ) < 0 ) {
    throw std::runtime_error( "sendto() failed" );
  }
}

Address UDPSocket::recvfrom( std::string& buf )
{
  buf.clear();
  buf.resize( BUFFER_LEN );

  Address::Raw saddr;
  socklen_t saddr_len = sizeof( saddr );

  ssize_t len;
  if ( ( len = ::recvfrom( fd, buf.data(), buf.size(), 0, saddr, &saddr_len ) ) < 0 ) {
    throw std::runtime_error( "recvfrom() failed" );
  }
  buf.resize( len );

  return { saddr, saddr_len };
}