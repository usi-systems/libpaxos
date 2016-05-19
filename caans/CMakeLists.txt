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

add_executable(server_caans learner.c ${SRCS})
target_link_libraries(server_caans netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(proxy_caans proposer.c ${SRCS})
target_link_libraries(proxy_caans netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(replica_caans replica.c ${SRCS})
target_link_libraries(replica_caans netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(client_caans client.c ${SRCS})
target_link_libraries(client_caans netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(coordinator_caans coordinator.c ${SRCS})
target_link_libraries(coordinator_caans netpaxos ${LIBPAXOS_LINKER_LIBS})