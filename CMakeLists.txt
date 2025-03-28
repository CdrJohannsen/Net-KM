cmake_minimum_required(VERSION 3.18)
project(Net-KM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(net-km-server "server.c")
add_executable(net-km-client "client.c")
