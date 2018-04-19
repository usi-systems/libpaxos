#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

if [ -f /etc/debian_version ]; then
    OS="Debian"
    VER=$(cat /etc/debian_version)
    apt-get install -y libevent-dev
elif [ -f /etc/redhat-release ]; then
    OS="Red Hat"
    VER=$(cat /etc/redhat-release)
    yum install -y libevent libevent-devel
fi

cd /tmp && wget https://github.com/msgpack/msgpack-c/releases/download/cpp-1.4.1/msgpack-1.4.1.tar.gz
mkdir -p msgpack/build && tar -xf msgpack-1.4.1.tar.gz -C msgpack --strip-components 1
cd msgpack/build
cmake ..
make
make install
ldconfig

# Install LevelDB Dependency
cd /tmp && git clone https://github.com/google/leveldb.git
cd leveldb
make
cp --preserve=links out-shared/libleveldb.* /usr/local/lib
cp -r include/leveldb /usr/local/include/
ldconfig

# Install RocksdB Dependency
sudo apt-get install libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
cd /tmp && git clone https://github.com/facebook/rocksdb.git
cd rocksdb
git checkout v5.12.2 -b stable
make static_lib
sudo make install

# Install DPDK Dependency
cd $HOME
git clone git://dpdk.org/dpdk
cd dpdk
export RTE_SDK=$HOME/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
git checkout 1ffee690eaa10b1b50deb230755ea4ceaa373e0f
make config T=$RTE_TARGET
sed -i 's/HPET=n/HPET=y/g' build/.config
make
mv build $RTE_TARGET
