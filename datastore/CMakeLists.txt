cmake_minimum_required (VERSION 2.8.4)
project (netpaxos)

find_package (RocksDB REQUIRED)
include_directories (${ROCKSDB_INCLUDE_DIRS})
set (PLATFORM_LDFLAGS stdc++ pthread rt snappy gflags z bz2 lz4 zstd numa m)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3 -O3 -Wall -Wextra -Wno-unused-parameter -fPIC")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
aux_source_directory (src SRCS)

set (TARGET datastore)
add_library (${TARGET} STATIC ${SRCS})
target_link_libraries (
    ${TARGET}
    ${ROCKSDB_LIBRARIES}
    ${PLATFORM_LDFLAGS}
)
set_target_properties (${TARGET} PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
