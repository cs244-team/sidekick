#pragma once

#include <sys/socket.h>

#include "address.hh"
#include <unistd.h>

class UDPSocket
{
private:
  int fd;
  static constexpr size_t BUFFER_LEN = 1500;

public:
  UDPSocket();
  ~UDPSocket();

  void bind( const Address& address );
  void sendto( std::string_view buf, const Address& address );
  Address recvfrom( std::string& buf );
};