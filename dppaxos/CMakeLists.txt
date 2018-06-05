cmake_minimum_required (VERSION 2.8.4)
project (p4xos)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(LIBEVENT REQUIRED)

find_package (dpdk REQUIRED)
include_directories (${DPDK_INCLUDE_DIR})

find_package (RocksDB REQUIRED)
include_directories (${ROCKSDB_INCLUDE_DIRS})
set (PLATFORM_LDFLAGS stdc++ pthread rt snappy gflags z bz2 lz4 zstd numa m)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3 -O0 -Wall -Wextra -Wno-unused-parameter -ggdb -g")

include_directories (${CMAKE_SOURCE_DIR}/paxos/include)
include_directories (${CMAKE_SOURCE_DIR}/p4xos/include)
include_directories (${CMAKE_SOURCE_DIR}/datastore/include)
include_directories (${CMAKE_SOURCE_DIR}/dppaxos/include)

aux_source_directory (src SRCS)
foreach(p ${SRCS})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
    target_link_libraries(${target}
    p4xos
    ${ROCKSDB_LIBRARIES}
	datastore
    ${LIBEVENT_LIBRARIES} # udp_client only
    ${PLATFORM_LDFLAGS})
    set_target_properties (${target} PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
endforeach(p)
