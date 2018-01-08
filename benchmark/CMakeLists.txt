include_directories(${CMAKE_SOURCE_DIR}/paxos/include)

# begin levelDB
# include_directories(/usr/local/include/leveldb/include)
# link_directories(/usr/local/lib)
# set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} leveldb)
# end LevelDB

set (LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} pthread)

set(TEST_SOURCES main.c single_thread.c multiple_learners.c)

foreach(p ${TEST_SOURCES})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
	target_link_libraries(${target} paxos ${LIBPAXOS_LINKER_LIBS})
endforeach(p)
