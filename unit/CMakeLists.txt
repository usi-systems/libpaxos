include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${CMAKE_SOURCE_DIR}/net/include)
include_directories(${CMAKE_SOURCE_DIR}/evpaxos/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

add_subdirectory(gtest)

add_executable(runtest runtest.cc acceptor_unittest.cc learner_unittest.cc
    config_unittest.cc replica_thread.c test_client.c replica_unittest.cc
    storage_unittest.cc messagepack_unittest.cc proposer_unittest.cc)

target_link_libraries(runtest netpaxos evpaxos pthread gtest-all)

add_test(unit-tests runtest)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink 
	${CMAKE_SOURCE_DIR}/unit/config
	${CMAKE_BINARY_DIR}/unit/config)
