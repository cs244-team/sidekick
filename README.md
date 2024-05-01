# CS244 Project

## Setup
```bash
sudo apt update
sudo apt install git cmake gdb build-essential \
     clang clang-tidy clang-format gcc-doc \
     pkg-config glibc-doc tcpdump tshark libpcap-dev
```


## To Run
```bash
cmake -S . -B build
cmake -—build build
```
Then, you can try out the playground app with 

```
./build/src/playground
```