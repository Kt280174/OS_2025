cmake_minimum_required(VERSION 3.10)
project(Connections)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Thêm warning
add_compile_options(-Wall -Werror)

# Các thư mục nguồn
set(CLIENT_SRC client)
set(HOST_SRC host)
set(CONNECTION Conn)
set(MODEL Config)

# Include
include_directories(
    ${CONNECTION}
    ${MODEL}
)

find_package(Threads)
find_library(LIBRT rt)

# Lấy danh sách các backend IPC
file(GLOB CONN_FILES ./conn/conn_*.cpp)

foreach(CONN_FILE ${CONN_FILES})
    get_filename_component(FILE_NAME ${CONN_FILE} NAME_WLE)
    string(REGEX REPLACE "conn_" "" CONN ${FILE_NAME})

    # ----------------
    # HOST (console)
    # ----------------
    add_executable(host_${CONN}
        ${HOST_SRC}/host.cpp     # main console
        ${HOST_SRC}/wolf_server.cpp
        ${HOST_SRC}/wolf_server.h
        ${MODEL}/game_config.h
        ${MODEL}/semaphores_manager.h
        ${CONNECTION}/conn.h
        ${CONNECTION}/conn_${CONN}.cpp
    )

    target_link_libraries(host_${CONN} pthread rt)

    # ----------------
    # CLIENT (console)
    # ----------------
    add_executable(client_${CONN}
        ${CLIENT_SRC}/client.cpp
        ${CLIENT_SRC}/goat_client.cpp
        ${CLIENT_SRC}/goat_client.h
        ${MODEL}/game_config.h
        ${MODEL}/semaphores_manager.h
        ${CONNECTION}/conn.h
        ${CONNECTION}/conn_${CONN}.cpp
    )

    target_link_libraries(client_${CONN} pthread rt)

endforeach()
