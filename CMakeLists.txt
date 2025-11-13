cmake_minimum_required(VERSION 3.15)
project(CppTcpMultiplayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
    if(MSVC)
        add_compile_options(/Od /Zi)
    else()
        add_compile_options(-g -O0)
    endif()
else()
    if(MSVC)
        add_compile_options(/O2)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

set(COMMON_SOURCES
    src/common/packet.cpp
)

function(setup_target target_name)
    target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    
    if(WIN32)
        target_link_libraries(${target_name} ws2_32)
    endif()
    
    set_target_properties(${target_name}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra)
    endif()
endfunction()

add_executable(echo_server
    src/server/echo_server.cpp
    src/server/message_queue.cpp
    src/server/server.cpp
    src/server/connection_manager.cpp
    ${COMMON_SOURCES}
)

add_executable(echo_client
    src/client/echo_client.cpp
    src/client/client.cpp
    ${COMMON_SOURCES}
)

add_executable(game_server
    src/server/game_server.cpp
    src/server/message_queue.cpp
    src/server/server.cpp
    src/server/connection_manager.cpp
    src/common/game_state.cpp
    src/common/serialization.cpp
    ${COMMON_SOURCES}
)

add_executable(game_client
    src/client/game_client.cpp
    src/client/client.cpp
    src/common/game_state.cpp
    src/common/serialization.cpp
    ${COMMON_SOURCES}
)

setup_target(echo_server)
setup_target(echo_client)
setup_target(game_server)
setup_target(game_client)