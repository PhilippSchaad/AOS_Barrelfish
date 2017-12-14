#/bin/bash

echo "check icmp ping response"
echo "ping 10.0.3.1 -c1 -s0"
ping 10.0.3.1 -c1 -s0

echo "check the udp echo server"
echo "nc -z -v -u 10.0.3.1 55"
nc -z -v -u 10.0.3.1 55
