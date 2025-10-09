cmake_minimum_required(VERSION 4.1.2)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(libmuduo LANGUAGES CXX)

set(CMAKE_CXX_MODULE_STD 1)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 26)
# 设置 C 标准
set(CMAKE_C_STANDARD 26)
# 设置编译器的参数
add_compile_options(-Wall -Wextra -Werror)

add_library(muduo STATIC)
target_sources( muduo

                PUBLIC
                FILE_SET CXX_MODULES FILES
                muduo.ccm
                core.ccm
                tcp_core.ccm
                core_forward.ccm
                tcp_core_forward.ccm
                logger.ccm
                timestamp.ccm
                epoll.ccm
                channel.ccm
                eventloop.ccm
                sock_addr.ccm
                socket.ccm
                acceptor.ccm
                buffer.ccm
                tcp_connection.ccm
                tcp_server.ccm
                eventloop_thread.ccm
                eventloop_threadpool.ccm

                PRIVATE
                logger.cxx
                timestamp.cxx
                epoll.cxx
                channel.cxx
                eventloop.cxx
                sock_addr.cxx
                socket.cxx
                acceptor.cxx
                buffer.cxx
                tcp_connection.cxx
                tcp_server.cxx
                eventloop_thread.cxx
                eventloop_threadpool.cxx
)

add_executable(main)
target_sources(main PRIVATE main.cxx)
target_link_libraries(  main
                        PRIVATE
                        muduo
)