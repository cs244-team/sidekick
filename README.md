# CS244 Project

## Setup
```bash
sudo apt update
sudo apt install git cmake gdb build-essential \
     clang clang-tidy clang-format gcc-doc \
     pkg-config glibc-doc tcpdump tshark libpcap-dev
```

## Build
```bash
cmake -S . -B build
cmake —-build build
```

To install the cryptography library we use, please see: https://libsodium.gitbook.io/doc/installation