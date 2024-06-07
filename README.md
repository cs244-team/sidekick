# CS244 Project

## Setup

To install the necessary packages for this project, run the following commands:

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

To reproduce the results of our replication, follow these steps:

1. Upload the `src/`, `util/`, and `scripts/` directories to your nearest Google Cloud server.
2. Install all the necessary dependencies outlined in the setup section on the server.
3. Run `scripts/run_server.sh` to listen for incoming packets over the WebRTC protocol and aggregate packets into the de-jitter buffer.
4. Run `scripts/run_proxy.sh` to run the sidekick proxy from the Raspberry Pi to send quACKs to the client.
5. Run `scripts/run_client.sh` to send outgoing packets to the server from your local laptop.

The results will be dumped into a CSV file for further examination.
