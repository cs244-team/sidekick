#include <fstream>
#include <queue>
#include <string>

using namespace std;

class AudioBuffer
{

private:
  queue<string> buffer {};

public:
  AudioBuffer() {};

  // From random source
  AudioBuffer( size_t num_samples, size_t sample_size )
  {
    string t;
    t.resize( sample_size );
    ifstream urandom( "/dev/urandom", ios::in | ios::binary );
    for ( size_t i = 0; i < num_samples; i++ ) {
      urandom.read( reinterpret_cast<char*>( t.data() ), sample_size ); // Read from urandom
      buffer.push( t );
    }
  }

  string pop()
  {
    string sample = buffer.front();
    buffer.pop();
    return sample;
  }

  void add_sample( string data ) { buffer.push( data ); }

  bool is_empty() { return buffer.empty(); }
};