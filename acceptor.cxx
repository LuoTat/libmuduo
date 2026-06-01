module;
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

module Muduo.Acceptor;

import Muduo.Logger;
import Muduo.Timestamp;

namespace ltt
{

namespace
{

int create_sockfd()
{
    LOG_FUNC_BEGIN();
    int sockfd {socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)};
    if (sockfd < 0)
    {
        LOG_FATAL("socket() failed! error:{}", std::strerror(errno));
    }
    LOG_FUNC_END();
    return sockfd;
}

}    // namespace

Acceptor::Acceptor(EventLoop* loop, SockAddress addr):
    m_loop {loop}, m_accept_socket {create_sockfd()}, m_accept_channel {m_accept_socket.get_fd(), loop->get_epoll()}
{
    LOG_FUNC_BEGIN();
    m_accept_socket.set_reuse_addr(true);    // 快速重启
    m_accept_socket.set_reuse_port(true);    // 多进程负载均衡
    m_accept_socket.bind(addr);
    LOG_FUNC_END();
}

Acceptor::~Acceptor()
{
    LOG_FUNC_BEGIN();
    m_accept_channel.del_all_event();
    m_accept_channel.remove();
    LOG_FUNC_END();
}

void Acceptor::listen()
{
    LOG_FUNC_BEGIN();
    m_is_listening = true;
    m_accept_socket.listen();

    // 当 EPoll 检测到 m_accept_socket 可读（有新连接）时
    // m_accept_channel 就会调用这里的回调函数
    m_accept_channel.set_read_callback(
        [this](Timestamp) -> void
        {
            LOG_FUNC_BEGIN("Acceptor REventCallback");
            SockAddress peer_addr;
            int         connfd {this->m_accept_socket.accept(peer_addr)};
            if (connfd > 0)
            {
                if (m_new_conn_cb)
                {
                    m_new_conn_cb(connfd, peer_addr);
                }
                else
                {
                    close(connfd);
                }
            }
            LOG_FUNC_END("Acceptor REventCallback");
        }
    );
    // 把 m_accept_channel 注册至 EPoll 中
    m_accept_channel.add_read_event();
    LOG_FUNC_END();
}

bool Acceptor::is_listening() const
{
    return m_is_listening;
}

void Acceptor::set_new_conn_callback(NewConnectionCallback cb)
{
    m_new_conn_cb = std::move(cb);
}

}    // namespace ltt
