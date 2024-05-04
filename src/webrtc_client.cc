#include <list>
#include <string>
#include <optional>
#include <string>
#include <mutex>
#include <chrono>
#include <sys/socket.h> 
#include <cstring> 
#include <iostream> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <vector.h>
#include <condition_variable>
#include "ipv4_datagram.hh"
#include "tcp_receiver_message.hh"

// draw this whole thing up and make it like a diagram and build it like legos

using namespace std;



class Client{

private:
  UDPSocket clientSocket; // Anymore initialization?
  UDPSocket quackSocket; // Anymore initialization?

  Address serverAddress;

  vector<IPv4Datagram> outstanding_dgrams {};
  queue<IPv4Datagram> audio_samples {};


  void retransmit(IPv4Datagram& datagram){
    clientSocket.sendto( serialize(datagram), serverAddress );
    // TODO: clean outstanding packets?
  }

  void send_packet(){
    IPv4Datagram& dgram = audio_samples.pop();

    // TODO: Encrypt packet


    clientSocket.sendto( serialize(dgram), serverAddress );
    outstanding_dgrams.push(dgram); // What info to add to outstanding? 
  }

  // Listening thread
  void receive_NACK(){
    string buf;
    serverSocket.recvfrom(buf);
    IPv4Datagram dgram;
    parse(dgram, buf);
    // Figure out which packet to retransmit 
    int seqno;
    for (const auto& line : dgram.payload) {
        // Find the substring 'seqno: '
        size_t pos = line.find("seqno: ");
        if (pos != string::npos) {
            // Extract the portion of the string after 'seqno: '
            string numberStr = line.substr(pos + "seqno: ".length());

            // Convert the extracted string to an integer
            seqno = stoi(numberStr);
        }
    }
    for (const auto& dg : outstanding_dgrams){
      if (dg.header.seqno == seqno){
        dgram = dg; 
        break;
      }
    }
    retransmit(dgram);
  }


public:
  Client(){
    clientSocket.bind( Address("0.0.0.0", 0) ); 
    quackSocket.bind( Address("0.0.0.0", 0) );
  }


}



int main( int argc, char* argv[] )
{
  return 0;
}