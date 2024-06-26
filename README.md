# CS244 Project

## Setup

To install the necessary packages for this project, run the following commands:

```bash
sudo apt update
sudo apt install git cmake gdb build-essential \
     clang clang-tidy clang-format gcc-doc \
     pkg-config glibc-doc tcpdump tshark libpcap-dev
```

To install the cryptography library we use, please see: https://libsodium.gitbook.io/doc/installation. To use an iPhone's personal hotspot with the Raspberry Pi 4 via USB tethering, [`libimobiledevice`](https://github.com/libimobiledevice/libimobiledevice) needs to be installed.

### Infrastructure

1. Set up a Raspberry Pi tethered to a cellular hotspot (we used 4G LTE).
2. Run a Google Cloud server in your nearest region.

## Build
Run the commands below to make and build the project:

```bash
cmake -S . -B build
cmake —-build build
```

## Mininet

Run the command below to start the emulation in Mininet:

```bash
sudo python3 evaluation/emulation.py
```

The parameters of the Mininet topology can be set within the `emulation.py` file for further experimentation.

## Sidekick

To reproduce the results of our replication, follow these steps:

1. Upload the `src/`, `util/`, and `scripts/` directories to your nearest Google Cloud server.
2. Install all the necessary dependencies outlined in the setup section on the server.
3. Run `scripts/run_server.sh` to listen for incoming packets over the WebRTC protocol and aggregate packets into the de-jitter buffer.
4. Run `scripts/run_proxy.sh` to run the sidekick proxy from the Raspberry Pi to send quACKs to the client.
5. Run `scripts/run_client.sh` to send outgoing packets to the server from your local laptop.

The results will be dumped into a CSV file for further examination.
