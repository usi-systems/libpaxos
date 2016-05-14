include_directories(${CMAKE_SOURCE_DIR}/sample/include)
include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/evpaxos/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

# begin levelDB
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)
set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} leveldb)
# end LevelDB

set(TEST_SOURCES acceptor.c proposer.c replica.c counter.c)

foreach(p ${TEST_SOURCES})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
	target_link_libraries(${target} evpaxos ${LIBPAXOS_LINKER_LIBS})
endforeach(p)

add_executable(learner learner.c application_config.c net_utils.c leveldb_context.c)
target_link_libraries(learner evpaxos ${LIBPAXOS_LINKER_LIBS})

add_executable(client client.c application_config.c net_utils.c)
target_link_libraries(client evpaxos ${LIBPAXOS_LINKER_LIBS})

INSTALL(PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/acceptor
	${CMAKE_CURRENT_BINARY_DIR}/proposer
	${CMAKE_CURRENT_BINARY_DIR}/replica
	${CMAKE_CURRENT_BINARY_DIR}/client
	DESTINATION bin)
