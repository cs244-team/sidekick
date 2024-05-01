/* The server in this scenario is the AWS Server/Myth Machine  */

#include <list>
#include <string>
#include <optional>
#include <string>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include "ipv4_datagram.hh"
#include "tcp_receiver_message.hh"



using namespace std;

class Server{
private:

  // Constructor has the buffer, RTT, and some sort of output queue 

  int RTT; 
  int time;
  mutex queueMutex;
  condition_variable dataCondition;

  int curr_seqno; 

  queue<string> buffer {};


  void send_NACK(int& seqno){
    
    // retransmit NACK up to one per RTT
    int curr_time = chrono::high_resolution_clock::now();
    if (curr_time - time < RTT){
      return;
    }

    // TODO: Make NACK 
    //         - involves adding seqno of every missing packet 
    TCPReceiverMessage message;
    
    time = chrono::high_resolution_clock::now();
    // TODO: Put NACK on the wire

  }

  // Server receives packet, must put it into queue 
  void receive_packet(IPv4Datagram& datagram){

    // TODO: Decrypt packet

    if ( message.SYN ) {
      curr_seqno = datagram.header.seqno;
    }

    // Case 1: nonconsecutive, send NACK back to client containing seqno of each missing packet
    if datagram.header.seqno != curr_seqno{
      send_NACK(datagram.header.seqno);
    } else { // Case 2: consecutive, add to output buffer
      lock_guard<mutex> lock(queueMutex);
      buffer.push(packet);
      dataCondition.notify_one();
    }
  }


  void packet_receiver(){
    while (true){
      IPv4Datagram& packet; 
      // TODO: Get the actual packet from the wire/check the wire 
      receive_packet(packet);
    }
  }


  void dataProcessor() {
    string data;
    while (true) {
      unique_lock<mutex> lock(queueMutex);
      dataCondition.wait(lock, []{ return !dataQueue.empty(); });
      data = buffer.front();
      buffer.pop();
      // TODO: Play back the data
    }
  }


}

int main( int argc, char* argv[] ){
  thread receiver(packet_receiver);
  thread processor(dataProcessor);
  receiver.join();
  processor.join();
  return 0;
}