module;
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

module Muduo.Acceptor;

import Muduo.Logger;
import Muduo.Timestamp;

namespace ltt
{

static int create_sockfd()
{
    int sockfd {socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)};
    if (sockfd < 0)
        LOG_FATAL("socket failed! error:{}", std::strerror(errno));
    return sockfd;
}

Acceptor::Acceptor(EPoll* epoll, const SockAddress& addr):
    m_epoll {epoll}, m_accept_socket {create_sockfd()}, m_accept_channel {epoll, m_accept_socket.get_fd()}
{
    m_accept_socket.set_reuse_addr(true);    // 快速重启
    m_accept_socket.set_reuse_port(true);    // 多进程负载均衡
    m_accept_socket.bind(addr);
}

Acceptor::~Acceptor()
{
    m_accept_channel.del_all_event();
    m_accept_channel.remove();
}

void Acceptor::listen()
{
    m_is_listenning = true;
    m_accept_socket.listen();

    // 当 EPoll 检测到 m_accept_socket 可读（有新连接）时
    // m_accept_channel 就会调用这里的回调函数
    m_accept_channel.set_read_callback(
        [this](Timestamp)
        {
            SockAddress peer_addr;
            int         connfd {this->m_accept_socket.accept(peer_addr)};
            if (connfd > 0)
            {
                if (m_new_conn_callback)
                    m_new_conn_callback(connfd, peer_addr);
                else
                    close(connfd);
            }
        });
    // 把 m_accept_channel 注册至 EPoll 中
    m_accept_channel.add_read_event();
}

bool Acceptor::listenning() const
{
    return m_is_listenning;
}

void Acceptor::set_new_conn_callback(NewConnectionCallback cb)
{
    m_new_conn_callback = std::move(cb);
}

}    // namespace ltt