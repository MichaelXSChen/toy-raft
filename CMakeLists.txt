cmake_minimum_required(VERSION 2.6.0)
project(tryRaft)
set(CMAKE_VERBOSE_MAKEFILE on)


set(CMAKE_CXX_FLAGS "-g -Wall -Werror -std=c++14")


find_package (Threads REQUIRED)
find_package (Protobuf REQUIRED)


include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROTOBUF_INCLUDE_DIRS})
message(${CMAKE_BINARY_DIR})

set(PROTOS
        "protobufs/echo.proto"
        "protobufs/raft.proto"
        )

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTOS})




#BRPC
find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if(EXAMPLE_LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
else()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
endif()
if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif()
include_directories(${BRPC_INCLUDE_PATH})


#GFLAG
find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif()
include_directories(${GFLAGS_INCLUDE_PATH})



#LEVEL DB
find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})

##SSL
find_library(SSL_LIB NAMES ssl)
if (NOT SSL_LIB)
    message(FATAL_ERROR "Fail to find ssl")
endif()

find_library(CRYPTO_LIB NAMES crypto)
if (NOT CRYPTO_LIB)
    message(FATAL_ERROR "Fail to find crypto")
endif()

set(DYNAMIC_LIB
        ${CMAKE_THREAD_LIBS_INIT}
        ${GFLAGS_LIBRARY}
        ${PROTOBUF_LIBRARIES}
        ${LEVELDB_LIB}
        ${SSL_LIB}
        ${CRYPTO_LIB}
        ${THRIFT_LIB}
        ${THRIFTNB_LIB}
        dl
        z
        )


#add_executable(test_server
#        src/testServer.cpp
#        ${PROTO_SRCS}
#        ${PROTO_HDRS}
#        )
#
#target_link_libraries(test_server
#        ${BRPC_LIB}
#        ${DYNAMIC_LIB}
#        )
#
#add_executable(test_client
#        src/testClient.cpp
#        ${PROTO_SRCS}
#        ${PROTO_HDRS}
#        )
#
#target_link_libraries(test_client
#        ${BRPC_LIB}
#        ${DYNAMIC_LIB}
#        )

add_executable(raft_server
        src/Node.cpp
        src/Node.hpp
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        src/Entry.cpp
        src/Entry.hpp
        src/main.cpp
        src/flags.hpp src/flags.cpp)


target_link_libraries(raft_server
        ${BRPC_LIB}
        ${DYNAMIC_LIB}
        )