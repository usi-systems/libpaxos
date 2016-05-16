cmake_minimum_required(VERSION 2.6)

include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

set(LOCAL_SOURCES netpaxos.c netutils.c proposer.c learner.c message_pack.c)

add_library(netpaxos SHARED ${LOCAL_SOURCES})
target_link_libraries(netpaxos paxos ${LIBPAXOS_LINKER_LIBS}
    ${LIBEVENT_LIBRARIES})