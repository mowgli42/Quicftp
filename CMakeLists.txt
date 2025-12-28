cmake_minimum_required(VERSION 3.15)
project(Quicftp VERSION 0.1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_CLIENT "Build client library and CLI" ON)
option(BUILD_SERVER "Build server library and CLI" ON)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(CLIENT_SOURCES
    quicftp_client.cc
    quicftpclient-cli.cc
)

set(SERVER_SOURCES
    quicftp_server.cc
    quicftpserver-cli.cc
)

# Common sources
set(COMMON_SOURCES
    quic_wrapper.cc
    stream_manager.cc
    test_bridge.cc
)

# Client library
if(BUILD_CLIENT)
    add_library(quicftp_client STATIC 
        quicftp_client.cc
        ${COMMON_SOURCES}
    )
    target_link_libraries(quicftp_client 
        OpenSSL::SSL 
        OpenSSL::Crypto
        Threads::Threads
    )
    
    add_executable(quicftpclient quicftpclient-cli.cc)
    target_link_libraries(quicftpclient quicftp_client)
endif()

# Server library
if(BUILD_SERVER)
    add_library(quicftp_server STATIC 
        quicftp_server.cc
        ${COMMON_SOURCES}
    )
    target_link_libraries(quicftp_server 
        OpenSSL::SSL 
        OpenSSL::Crypto
        Threads::Threads
    )
    
    add_executable(quicftpserver quicftpserver-cli.cc)
    target_link_libraries(quicftpserver quicftp_server)
endif()

# TODO: Add ngtcp2 when available
# For now, we'll build without it and add it later
# find_package(ngtcp2 REQUIRED)
# target_link_libraries(quicftp_client ngtcp2::ngtcp2)
# target_link_libraries(quicftp_server ngtcp2::ngtcp2)

# Installation
install(TARGETS quicftpclient quicftpserver
    RUNTIME DESTINATION bin
)

install(FILES quicftp_client.h quicftp_server.h
    DESTINATION include/quicftp
)

