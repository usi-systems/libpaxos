#!/bin/bash -e

LIBDIR=$HOME/libraries
mkdir -p $LIBDIR

if [ -f /etc/debian_version ]; then
    OS="Debian"
    VER=$(cat /etc/debian_version)
    sudo apt-get update && sudo apt-get install -y build-essential git cmake libevent-dev
elif [ -f /etc/redhat-release ]; then
    OS="Red Hat"
    VER=$(cat /etc/redhat-release)
    sudo yum install -y libevent libevent-devel
fi

cd $LIBDIR && git clone https://github.com/msgpack/msgpack-c.git msgpack
cd msgpack
git checkout -b 1.4.1 cpp-1.4.1
mkdir -p build
cd build
cmake ..
make
sudo make install
sudo ldconfig

# Install LevelDB Dependency
cd $LIBDIR && git clone https://github.com/google/leveldb.git
cd leveldb
git checkout v1.20
make
sudo cp --preserve=links out-shared/libleveldb.* /usr/local/lib
sudo cp -r include/leveldb /usr/local/include/
sudo ldconfig

# Install RocksdB Dependency
sudo apt-get install -y libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
cd $LIBDIR && git clone https://github.com/facebook/rocksdb.git
cd rocksdb
git checkout v5.12.2 -b stable
make static_lib
sudo make install

# Install DPDK Dependency
sudo apt-get install -y libnuma-dev
cd $LIBDIR
git clone git://dpdk.org/dpdk
cd dpdk
export RTE_SDK=$LIBDIR/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
git checkout v18.08
make config T=$RTE_TARGET O=$RTE_TARGET
sed -i 's/HPET=n/HPET=y/g' $RTE_SDK/$RTE_TARGET/.config
make O=$RTE_TARGET
