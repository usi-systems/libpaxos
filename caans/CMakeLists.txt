include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${CMAKE_SOURCE_DIR}/caans/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

set (SRCS configuration.c application_proxy.c)

# begin levelDB
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)
set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} leveldb)
# end LevelDB

message(${SRCS})

add_executable(appserver learner.c ${SRCS})
target_link_libraries(appserver netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(appclient proposer.c ${SRCS})
target_link_libraries(appclient netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(appreplica replica.c ${SRCS})
target_link_libraries(appreplica netpaxos ${LIBPAXOS_LINKER_LIBS})
