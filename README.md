## Dependencies

The code was compiled successfully on servers with Ubuntu 16.04 (Xenial).

The `install_deps.sh` script installs neccessary dependencies for Libpaxos.
The script also installs LevelDB, RocksDB and DPDK.

```
git clone https://github.com/usi-systems/libpaxos.git
cd libpaxos
export LIBPAXOS_SRC=`pwd`
./install_deps.sh
```

## Environment Variables

Run the following commands or add them in the `.bashrc` or `.bash_profile`

```
export RTE_SDK=$HOME/libraries/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
```

## Building

These are the basic steps required to compile Libpaxos.
Here, we build the executables out of the source code (e.g., the build directory is
located at `$HOME`

```
mkdir -p $HOME/build/libpaxos
cd $HOME/build/libpaxos
export LIBPAXOS_BIN=$HOME/build/libpaxos
cmake $LIBPAXOS_SRC
make
```

## Run Libpaxos (UDP socket)

relocate to the build directory (e.g., $LIBPAXOS_BIN as specified above):


### Leader (aka. Coordinator)

a sample configuration `application.conf` is included in the top directory of libpaxos.

Usage:
```
$LIBPAXOS_BIN/caans/sw_coordinator <configuration-file>
```

e.g.,

```
$LIBPAXOS_BIN/caans/sw_coordinator $LIBPAXOS_SRC/application.conf
```


### Acceptor

Usage:
```
$LIBPAXOS_BIN/caans/sw_acceptor <configuration-file> <acceptor-id>
```
e.g.,

```
$LIBPAXOS_BIN/caans/sw_acceptor  $LIBPAXOS_SRC/application.conf 0
```

## Run DPDK Paxos

First, you need to reserve Hugepages and bind a NIC that supports DPDK (you
could refer to DPDK documentation or use the script `init_dpdk.sh` as example.

```
sudo -E ./init_dpdk.sh
```

You can configure DPDK Paxos with a number of parameters, such as port-core
affinity,  worker cores assignment, and burst size, etc. Please refer to the
usage of each program for details.

### Leader

```
sudo $LIBPAXOS_BIN/dppaxos/dpdk_leader -c ff -n 4  --socket-mem 256
--file-prefix ld --log-level 7 -- \
--rx "(0,0,0)(1,0,0)" --tx "(0,1)(1,1)" --w "2,3,4,5" --pos-lb 43 \
--lpm "192.168.4.0/24=>1;224.0.0.0/4=>2;" \
--bsz "(1,1), (1,1), (1,1)" --msgtype 2 --osd 16 --multi-dbs
```

or you could use the script

```
$LIBPAXOS_SRC/start_leader.sh
```

### Acceptor

```
sudo $LIBPAXOS_BIN/dppaxos/dpdk_acceptor -c ff -n 4  --socket-mem 256
--file-prefix ac --log-level 7 -- \
--rx "(0,0,0)(1,0,0)" --tx "(0,1)(1,1)" --w "2,3,4,5" --pos-lb 43 \
--lpm "192.168.4.0/24=>1;224.0.0.0/4=>2;" \
--bsz "(1,1), (1,1), (1,1)" --msgtype 2 --osd 16 --multi-dbs
```


or you could use the script

```
$LIBPAXOS_SRC/start_acceptor.sh
```

### Replica
The Replica replicates RocksDB instance, you will need to put a configuration
for RocksDB in the directory that you will run the script. A sample configuration
is `rocksdb.conf`located in `$LIBPAXOS_SRC`.


```
$LIBPAXOS_SRC/start_replica.sh
```

## License

LibPaxos is distributed under the terms of the 3-clause BSD license.
LibPaxos has been developed at the Universita\` della Svizzera Italiana
by [Daniele Sciascia][https://www.linkedin.com/in/daniele-sciascia/?originalSubdomain=ch]
and later extended by [Huynh Tu Dang][http://tudang.github.io].
