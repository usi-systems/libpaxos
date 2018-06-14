cmake_minimum_required (VERSION 2.8.4)
project (p4xos)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package (dpdk REQUIRED)
include_directories (${DPDK_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/include)

set (RTE_TARGET x86_64-native-linuxapp-gcc)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3 -O0 -Wall -Wextra -Wno-unused-parameter -ggdb -g")

option (DEFINE_RATE_LIMITER "Option enable rate limiter" OFF) # Disabled by default
if (DEFINE_RATE_LIMITER)
    message("Adding RATE_LIMITER flag...")
    add_definitions(-DRATE_LIMITER)
endif (DEFINE_RATE_LIMITER)

option (DEFINE_RESUBMIT "Option enable resubmit for proposer" OFF) # Disabled by default
if (DEFINE_RESUBMIT)
    message("Adding RESUBMIT flag...")
    add_definitions(-DRESUBMIT)
endif (DEFINE_RESUBMIT)

include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/p4xos/include)

aux_source_directory (src SRCS)
set (TARGET p4xos)

add_library (${TARGET} ${SRCS})
target_link_libraries (${TARGET}
    paxos
    ${DPDK_LIBRARIES})
set_target_properties (${TARGET} PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
