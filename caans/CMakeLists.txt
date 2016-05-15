include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

# begin levelDB
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)
set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} leveldb)
# end LevelDB

add_executable(replica replica.c)
target_link_libraries(replica netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(proposer proposer.c)
target_link_libraries(proposer netpaxos ${LIBPAXOS_LINKER_LIBS})
