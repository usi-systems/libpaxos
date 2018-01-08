include_directories(${CMAKE_SOURCE_DIR}/paxos/include)

#set (ROCKSDB_SDK /mnt/data/rocksdb)
#include_directories(${ROCKSDB_SDK})
#include_directories(${ROCKSDB_SDK}/include)
#link_directories(${ROCKSDB_SDK})

find_package(RocksDB REQUIRED)
include_directories(${ROCKSDB_INCLUDE_DIRS})
set (PLATFORM_LDFLAGS stdc++ pthread rt snappy gflags z bz2 lz4 zstd numa)
include_directories(${CMAKE_SOURCE_DIR}/rockspaxos/include)


add_library(rocksdb_context rocksdb_context.c)
target_link_libraries(rocksdb_context paxos ${ROCKSDB_LIBRARIES} ${PLATFORM_LDFLAGS})
