set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wno-unused-parameter -ggdb -g")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(LIBEVENT REQUIRED)

message (STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})

include_directories (${CMAKE_SOURCE_DIR}/datastore/include)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable (server src/server.c src/paxos.c)
target_link_libraries (server ${LIBEVENT_LIBRARIES} datastore)
set_target_properties (server PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})

add_executable (client src/client.c  src/paxos.c)
target_link_libraries (client ${LIBEVENT_LIBRARIES} datastore)
set_target_properties (client PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
