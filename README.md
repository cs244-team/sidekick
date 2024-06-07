# CS244 Project

## Setup
Run the commands below to install the necessary packages for this project

```bash
sudo apt update
sudo apt install git cmake gdb build-essential \
     clang clang-tidy clang-format gcc-doc \
     pkg-config glibc-doc tcpdump tshark libpcap-dev
```

To install the cryptography library we use, please see: https://libsodium.gitbook.io/doc/installation

## Build
Run the commands below to make and build the project

```bash
cmake -S . -B build
cmake —-build build
```

## Mininet

[TODO]

## Sidekick

In order to reproduce the results of our replication, upload the `src/`, `util/`, and `scripts/` directories to your nearest Google Cloud server and
install all the necessary dependancies outlined above within the server. 

Run `scripts/run_server.sh` to listen for incoming packets over the WebRTC protocol and aggregate packets into the de-jitter buffer.

Run `scripts/run_proxy` to run the sidekick proxy from the Raspberry Pi to send quACKs to the client. 

Run `scripts/run_client.sh` to send outgoing packets to the server from your local laptop.

The results will be dumped into a csv file for further examination.