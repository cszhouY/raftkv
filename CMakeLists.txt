cmake_minimum_required (VERSION 3.16)
project (kvstoresystem)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

aux_source_directory(src DIR_SRCS)
# aux_source_directory(test DIR_TESTS)
SET(EXECUTABLE_OUTPUT_PATH .)

find_library(LIBRPC rpc PATHS lib)

if(${TEST})
	message("TEST: complie test/server.cpp and test/client.cpp")
	add_executable(test_timer ./test/test_timer.cpp)
	target_link_libraries(test_timer pthread)
	add_executable(test_db ./test/test_db.cpp ./src/KVDB.cpp)
else()
	message("DEBUG: compile ./src for kvstoresystem")
	add_executable(kvstoresystem ${DIR_SRCS})
	target_link_libraries(kvstoresystem ${LIBRPC} pthread)
endif()
