"""Mininet-based evaluation of Sidekick

Topology:
    client -- router -- server
           l1        l2

Link properties:
    - l1: high loss (3.6%), high bandwidth (100 Mbit/s),  low delay ( 1 ms)
    - l2:  low loss (  0%),  low bandwidth ( 10 Mbit/s), high delay (25 ms)
"""

from mininet.cli import CLI
from mininet.link import TCIntf
from mininet.log import setLogLevel
from mininet.net import Mininet, Node
from mininet.topo import Topo

# Test parameters (could make these arguments)
L1_DELAY = 1  # milliseconds
L1_LOSS = 3.6  # percentage
L1_BANDWIDTH = 100  # Mbit/s
L2_DELAY = 25
L2_LOSS = 0
L2_BANDWIDTH = 10

EXEC_DIR = "../build/src/"


class Router(Node):
    """Node with IP forwarding

    Ref: https://github.com/mininet/mininet/blob/master/examples/linuxrouter.py
    """

    def config(self, **kwargs):
        super().config(**kwargs)
        self.cmd("sysctl net.ipv4.ip_forward=1")

    def terminate(self):
        self.cmd("sysctl net.ipv4.ip_forward=0")
        super().terminate()


class NetworkTopo(Topo):
    def build(self, **kwargs):
        client = self.addHost("client")
        router = self.addNode("router", cls=Router)
        server = self.addHost("server")

        l1_params = {"delay": f"{L1_DELAY}ms", "bw": L1_BANDWIDTH, "loss": L1_LOSS}
        l2_params = {"delay": f"{L2_DELAY}ms", "bw": L2_BANDWIDTH, "loss": L2_LOSS}

        self.addLink(client, router, intf=TCIntf, params1=l1_params, params2=l1_params)
        self.addLink(router, server, intf=TCIntf, params1=l2_params, params2=l2_params)


def configure_network(network: Mininet):
    client, router, server = network["client"], network["router"], network["server"]

    router.setIP("192.168.0.1", intf="router-eth0")
    router.setIP("35.240.0.1", intf="router-eth1")

    client.setIP("192.168.0.2", intf="client-eth0")
    client.cmd("ip route add default via 192.168.0.1")

    server.setIP("35.240.20.220", intf="server-eth0")
    server.cmd("ip route add default via 35.240.0.1")


def run_commands(network: Mininet):
    client, router, server = network["client"], network["router"], network["server"]
    run_webrtc_client(node=client, server_ip=server.IP())
    run_sidekick_proxy(node=router, interface="router-eth0")
    run_webrtc_server(node=server, rtt=2 * (L1_DELAY + L2_DELAY))

    CLI(network)


def run_webrtc_client(node, server_ip):
    cmd = f"{EXEC_DIR}/webrtc_client --server-ip {server_ip} > ./logs/webrtc_client.log 2>&1 &"
    return node.cmd(cmd)


def run_sidekick_proxy(node, interface, quacking_interval=2, threshold=8):
    cmd = (
        f"{EXEC_DIR}/sidekick_proxy --interface {interface} "
        f"--filter 'ip and udp and not dst net 192.168 and not dst net 224' "
        f"--quack {quacking_interval} "
        f"--threshold {threshold} "
        f"> ./logs/sidekick_proxy.log 2>&1 &"
    )
    return node.cmd(cmd)


def run_webrtc_server(node, rtt, port=9000):
    cmd = f"{EXEC_DIR}/webrtc_server --rtt {rtt} --port {port} > ./logs/webrtc_server.log 2>&1 &"
    return node.cmd(cmd)


def run_test():
    topo = NetworkTopo()
    network = Mininet(topo=topo)

    configure_network(network)
    # network.pingAll()
    network.start()
    run_commands(network)
    network.stop()


if __name__ == "__main__":
    # setLogLevel("info")
    run_test()
