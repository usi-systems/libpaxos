include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${CMAKE_SOURCE_DIR}/caans/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

set (SRCS configuration.c application_proxy.c message.c)

# begin levelDB
#include_directories(/usr/local/include/leveldb/include)
#link_directories(/usr/local/lib)
find_package(LevelDB REQUIRED)
# set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS})
# end LevelDB

find_package(RocksDB REQUIRED)
include_directories(${ROCKSDB_INCLUDE_DIRS})
set (PLATFORM_LDFLAGS stdc++ pthread rt snappy gflags z bz2 lz4 zstd numa m)

message(${SRCS})

add_executable(server_caans learner.c leveldb_context.c ${SRCS})
target_link_libraries(server_caans netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(proxy_caans proposer.c ${SRCS})
target_link_libraries(proxy_caans netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(replica_caans replica.c ${SRCS})
target_link_libraries(replica_caans netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(client_caans client.c ${SRCS})
target_link_libraries(client_caans netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(c_bench c_bench.c ${SRCS})
target_link_libraries(c_bench netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(pktgen4learner pktgen4learner.c ${SRCS})
target_link_libraries(pktgen4learner netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(sw_coordinator coordinator.c ${SRCS})
target_link_libraries(sw_coordinator netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(sw_acceptor acceptor.c ${SRCS})
target_link_libraries(sw_acceptor netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(sequencer sequencer.c ${SRCS})
target_link_libraries(sequencer netpaxos ${LIBPAXOS_LINKER_LIBS} ${LEVELDB_LIBS})

add_executable(rock_learner rock_learner.c rocksdb_context.c ${SRCS})
target_link_libraries(rock_learner netpaxos ${LIBPAXOS_LINKER_LIBS} ${ROCKSDB_LIBRARIES} ${PLATFORM_LDFLAGS})
