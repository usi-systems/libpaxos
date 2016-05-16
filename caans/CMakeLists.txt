include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

# begin levelDB
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)
set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} leveldb)
# end LevelDB

add_executable(appserver replica.c)
target_link_libraries(appserver netpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(appclient proposer.c)
target_link_libraries(appclient netpaxos ${LIBPAXOS_LINKER_LIBS})
