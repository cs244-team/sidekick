#include <string>
#include <queue>

using namespace std;


class AudioBuffer{

private:

    queue<string> buffer {};


public:
    AudioBuffer() {}

    string pop(){ 
        string sample = buffer.front();
        buffer.pop(); 
        return sample;
    }

    void add_sample(string data){ buffer.push(data); }

    bool is_empty(){ return buffer.empty(); }

};