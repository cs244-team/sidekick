#include <string>
#include <queue>

using namespace std;


class AudioBuffer{

private:

    queue<string> buffer {};


public:
    AudioBuffer() {}

    string pop(){ return buffer.pop(); }

    void add_sample(string data){ buffer.push(data); }

}