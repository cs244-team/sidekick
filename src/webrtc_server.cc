/* The server in this scenario is the AWS Server/Myth Machine  */

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
#include <pair>
#include <tuple>
#include <algorithm>
#include <unistd.h> 
#include <condition_variable>
#include "ipv4_datagram.h"
#include "tcp_receiver_message.h"
#include "jitter_buffer.h"

// draw this whole thing up and make it like a diagram and build it like legos

using namespace std;

class Server{


private:

  // Constructor has the buffer, RTT, and some sort of output queue 

  int RTT = 0; 

  vector<pair<int, int>> NACK_packets {}; // <seqno, time>
  // int time = 0;
  mutex vectorMutex {};
  condition_variable dataCondition {};

  UDPSocket serverSocket {};
  Address clientAddress {};

  int curr_seqno = 0; 




  void send_NACK(int& seqno){

    int curr_time = chrono::high_resolution_clock::now();

    // Retransmit NACK for >1 RTT 
    for(auto& elem : NACK_packets){
      if (curr_time - elem.second > RTT){
        IPv4Datagram dgram;
        string payload = "Seqno: " + elem.first;
        dgram.payload.push_back(payload);
        serverSocket.sendto( serialize(dgram.payload) , clientAddress);

        // Edit vector
        NACK_packets.second = curr_time;
      }
    }

    // Add current to retransmit 
    for (int i = curr_seqno; i < seqno; i++){
      // Put NACK on wire
      IPv4Datagram dgram;
      string payload = "Seqno: " + i;
      dgram.payload.push_back(payload);
      serverSocket.sendto( serialize(dgram.payload) , clientAddress);

      // Add to NACK packets vector
      NACK_packets.push_back(make_pair(seqno, curr_time))  
    }
  }

  string extract_data(IPv4Datagram& datagram){
      // Then add for what you just got 
    for (const auto& line : datagram.payload) {
      // Find the substring 'seqno: '
      size_t pos = line.find("data: ");
      if (pos != string::npos) {
          // Extract the portion of the string after 'seqno: '
          string numberStr = line.substr(pos + "data: ".length());

          // Convert the extracted string to an integer
          return stoi(numberStr);
      }
    }
    return "";
  }

  // Server receives packet, must put it into queue 
  void receive_packet(IPv4Datagram& datagram){

    // TODO: Decrypt packet

    // Probably not SYN per se but just need some notion of starting seqno 
    if ( message.SYN ) {
      curr_seqno = datagram.header.seqno;
    }

    buffer.push(datagram.header.seqno, IPv4Datagram::parse(datagram.payload));

    // Case 1: nonconsecutive, send NACK back to client containing seqno of each missing packet
    if datagram.header.seqno != curr_seqno + 1{ // Probably > curr_seqno
      send_NACK(datagram.header.seqno); 
      
      // Update buffer
      

      // First, add empty spots
      for(int i = curr_seqno+1; i < datagram.header.seqno; i++){
        lock_guard<mutex> lock(vectorMutex);
        buffer.push_back(make_tuple("", i, false));
      }

      // Second, add data you received
      string data = extract_data(datagram);
      buffer.push_back(make_tuple(data, datagram.header.seqno, true));


    } else { // Case 2: consecutive, add to output buffer
      curr_seqno = datagram.header.seqno; // Update curr seqno
      lock_guard<mutex> lock(vectorMutex);

      string data = extract_data(datagram);


      // TODO: this seems super inefficient to update the vector with placeholders but for now....
      // Could probably keep a bool if you're waiting on data to skip over this call or something

      auto it = std::find_if(buffer.begin(), buffer.end(), [curr_seqno](const std::tuple<std::string, int, std::string>& tup) {
        return std::get<1>(tup) == curr_seqno;  // Check if the second element of the tuple is seqno
      });

      // If already in buffer because missing with NACK, update
      if(it != buffer.end()){ 
        it->first = data;
        it->third = true;
      } else{ // Else, push back
        buffer.push_back(make_tuple(data, curr_seqno, true));
      }
      dataCondition.notify_one();
      // Edit the outstanding vector
      for (auto it = NACK_packets.begin(); it != NACK_packets.end(); ) {
          if (it->first < curr_seqno) {
              // Erase returns the iterator to the next element
              it = NACK_packets.erase(it);
          } else {
              // Only increment the iterator if no erasure happens
              ++it;
          }
      }
    }
  }


  void packet_receiver(){
    while (true){
      IPv4Datagram dgram; 
      // Get the actual packet from the wire/check the wire 
      string buf;
      clientAddress = serverSocket.recvfrom(buf);
      // Convert to IPv4 Datagram
      parse(dgram, buf);

      receive_packet(dgram);
    }
  }


  void play_data(string& data){
    // TODO: play the data
  }

  // TODO: Edit this to work with vector logic 
  void dataProcessor() {
    string data;
    while (true) {
      unique_lock<mutex> lock(vectorMutex);
      dataCondition.wait(lock, []{ 
        return !buffer.empty() && buffer.front().third == true; 
      });

      while (!buffer.empty() && buffer.front().third) {
        play_data(buffer.front().first);
        buffer.erase(buffer.begin());  // Remove the processed item from the vector
      }
    }
  }


  // int RTT; 
  // int time;
  // mutex queueMutex;
  // condition_variable dataCondition;

  // UDPSocket serverSocket {};

  // int curr_seqno; 

  // queue<string> buffer {};


  public:

  // Add parameters:
  //    - RTT
  JitterBuffer buffer {};

  Server()
  {

    serverSocket.bind( Address("0.0.0.0", 0) ); // Can change

  }
}

int main( int argc, char* argv[] ){
  thread receiver(packet_receiver);
  thread processor(dataProcessor);
  receiver.join();
  processor.join();
  return 0;
}