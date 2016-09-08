cmake_minimum_required(VERSION 2.6)

include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces")

set(LOCAL_SOURCES netpaxos.c netutils.c proposer.c learner.c
    message_pack.c replica.c coordinator.c acceptor.c sequencer.c )

add_library(netpaxos SHARED ${LOCAL_SOURCES})
target_link_libraries(netpaxos paxos ${LIBPAXOS_LINKER_LIBS}
    ${LIBEVENT_LIBRARIES})