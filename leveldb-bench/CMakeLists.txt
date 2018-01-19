include_directories(${CMAKE_SOURCE_DIR}/paxos/include)

# begin levelDB
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)

set(TEST_SOURCES multi_thread.c multi_thread_multi_db.c)

foreach(p ${TEST_SOURCES})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
	target_link_libraries(${target} leveldb pthread)
endforeach(p)
