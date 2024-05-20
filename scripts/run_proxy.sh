sudo ./build/src/sidekick_proxy \
    --interface enp0s1 \
    --filter "ip and udp and not dst net 192.168 and not dst net 224 and dst port 9000" \
    --quack 2 \
    --threshold 8