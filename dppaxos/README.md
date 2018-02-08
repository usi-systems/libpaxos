# Dependencies
```
sudo apt install libnuma-dev libpcap-dev libboost-filesystem1.58-dev libboost-program-options1.58-dev
```

Required DPDK commit 1ffee690eaa10b1b50deb230755ea4ceaa373e0f

# Build
```
  mkdir build && cd build
  cmake .. && make
```

# Run
```
sudo ./main
```
