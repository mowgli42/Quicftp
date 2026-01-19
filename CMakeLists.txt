cmake_minimum_required(VERSION 3.15)
project(Quicftp VERSION 0.1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_CLIENT "Build client library and CLI" ON)
option(BUILD_SERVER "Build server library and CLI" OFF) # Server is now Caddy (Docker)

# Find Curl (Custom build or system)
# We expect the user to run scripts/build_curl_http3.sh to populate deps/dist
set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/dist")
if(EXISTS "${DEPS_DIR}")
    message(STATUS "Using custom libcurl build from ${DEPS_DIR}")
    include_directories(${DEPS_DIR}/include)
    link_directories(${DEPS_DIR}/lib)
    set(CURL_LIBRARY "${DEPS_DIR}/lib/libcurl.a")
    
    # We also need to link dependencies of static curl if not using shared
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(Threads REQUIRED)
else()
    message(STATUS "Using system libcurl")
    find_package(CURL REQUIRED)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Client library
if(BUILD_CLIENT)
    add_library(quicftp_client STATIC 
        quicftp_client.cc
    )
    
    if(EXISTS "${DEPS_DIR}")
        target_link_libraries(quicftp_client 
            ${CURL_LIBRARY}
            OpenSSL::SSL 
            OpenSSL::Crypto
            ZLIB::ZLIB
            Threads::Threads
            -ldl -lpthread
        )
    else()
        target_link_libraries(quicftp_client CURL::libcurl)
    endif()
    
    add_executable(quicftpclient quicftpclient-cli.cc)
    target_link_libraries(quicftpclient quicftp_client)
endif()

# Installation
install(TARGETS quicftpclient
    RUNTIME DESTINATION bin
)

install(FILES quicftp_client.h
    DESTINATION include/quicftp
)
