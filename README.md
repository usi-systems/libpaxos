## Dependencies

The code was compiled successfully on servers with Ubuntu 16.04 (Xenial).

The `install_deps.sh` script installs neccessary dependencies for Libpaxos.
The script also installs LevelDB, RocksDB and DPDK.

```
git clone https://github.com/usi-systems/libpaxos.git
cd libpaxos
./install_deps.sh
```

## Environment Variables

Run the following commands or add them in the `.bashrc` or `.bash_profile`

```
export RTE_SDK=$HOME/libraries/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
```

## Building

These are the basic steps required to compile Libpaxos

```
mkdir build
cd build
cmake ..
make
```

## License

LibPaxos is distributed under the terms of the 3-clause BSD license.
LibPaxos has been developed at the Universita\` della Svizzera Italiana
by [Daniele Sciascia][https://www.linkedin.com/in/daniele-sciascia/?originalSubdomain=ch]
and later extended by [Huynh Tu Dang][http://tudang.github.io].
