sudo ip route add 224.0.0.0/4 dev eth1
sudo ethtool -K eth1 tx off rx off gso off gro off
sudo bash -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
