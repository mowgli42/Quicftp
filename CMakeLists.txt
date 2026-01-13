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

# ngtcp2 and nghttp3 libraries (installed system-wide)
# These are C libraries, so we link them directly
find_library(NGTCP2_LIB ngtcp2 PATHS /usr/lib /usr/local/lib REQUIRED)
find_library(NGTCP2_CRYPTO_OSSL_LIB ngtcp2_crypto_ossl PATHS /usr/lib /usr/local/lib REQUIRED)
find_library(NGHTTP3_LIB nghttp3 PATHS /usr/lib /usr/local/lib REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(/usr/include/ngtcp2 /usr/include/nghttp3)

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
        ${NGTCP2_LIB}
        ${NGTCP2_CRYPTO_LIB}
        ${NGHTTP3_LIB}
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
        ${NGTCP2_LIB}
        ${NGTCP2_CRYPTO_LIB}
        ${NGHTTP3_LIB}
    )
    
    add_executable(quicftpserver quicftpserver-cli.cc)
    target_link_libraries(quicftpserver quicftp_server)
endif()


# Installation
install(TARGETS quicftpclient quicftpserver
    RUNTIME DESTINATION bin
)

install(FILES quicftp_client.h quicftp_server.h
    DESTINATION include/quicftp
)

