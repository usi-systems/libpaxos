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