module;
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

module Muduo.Socket;

import Muduo.Logger;

namespace ltt
{

Socket::Socket(int sockfd): m_sockfd {sockfd}
{}

Socket::~Socket()
{
    close(m_sockfd);
}

void Socket::bind(SockAddress addr) const
{
    LOG_FUNC_BEGIN();
    auto sock_addr {addr.get_sock_addr()};
    if (::bind(m_sockfd, std::bit_cast<sockaddr*>(&sock_addr), sizeof(sockaddr_in)) != 0)
    {
        LOG_FATAL("bind() failed! error:{}", std::strerror(errno));
    }
    LOG_INFO("socket of fd:{} has bind to [{}]", m_sockfd, addr.get_ip_with_port());
    LOG_FUNC_END();
}

void Socket::listen() const
{
    LOG_FUNC_BEGIN();
    if (::listen(m_sockfd, 1024) != 0)
    {
        LOG_FATAL("listen() failed! error:{}", std::strerror(errno));
    }
    LOG_INFO("socket of fd:{} is listening", m_sockfd);
    LOG_FUNC_END();
}

int Socket::accept(SockAddress& peer_addr) const
{
    LOG_FUNC_BEGIN();
    sockaddr_in peer_addr_in {};
    socklen_t   len {sizeof(peer_addr_in)};
    int         connfd {accept4(m_sockfd, std::bit_cast<sockaddr*>(&peer_addr_in), &len, SOCK_NONBLOCK | SOCK_CLOEXEC)};
    if (connfd > 0)
    {
        peer_addr.set_sock_addr(peer_addr_in);
        LOG_INFO("socket of fd:{} has accepted a new connection from [{}]", m_sockfd, peer_addr.get_ip_with_port());
    }
    else
    {
        LOG_ERROR("accept4 failed! error:{}", std::strerror(errno));
    }
    LOG_FUNC_END();
    return connfd;
}

void Socket::shutdown_write() const
{
    LOG_FUNC_BEGIN();
    if (shutdown(m_sockfd, SHUT_WR) == 0)
    {
        LOG_INFO("socket of fd:{} has shutdown write", m_sockfd);
    }
    else
    {
        if (errno == ENOTCONN)
        {
            LOG_INFO("socket of fd:{} is not connected, no need to shutdown write", m_sockfd);
        }
        else
        {
            LOG_ERROR("shutdown_write failed! error:{}", std::strerror(errno));
        }
        LOG_FUNC_END();
    }
}

int Socket::get_fd() const
{
    return m_sockfd;
}

void Socket::set_tcp_no_delay(bool flag) const
{
    // TCP_NODELAY 用于禁用 Nagle 算法
    // Nagle 算法用于减少网络上传输的小数据包数量
    // 将 TCP_NODELAY 设置为 1 可以禁用该算法,允许小数据包立即发送
    int optval {flag ? 1 : 0};
    setsockopt(m_sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::set_reuse_addr(bool flag) const
{
    // SO_REUSEADDR 允许一个套接字绑定到一个已经被 TIME_WAIT 状态 占用的端口上
    // 当服务器程序崩溃或重启后，需要立即绑定到之前的端口
    // 防止因端口被 TIME_WAIT 占用导致 bind 失败
    int optval {flag ? 1 : 0};
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::set_reuse_port(bool flag) const
{
    // SO_REUSEPORT 允许多个套接字在同一主机上绑定到同一个端口
    // 多进程/多线程的服务器负载均衡,每个进程绑定同一端口
    int optval {flag ? 1 : 0};
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::set_keep_alive(bool flag) const
{
    // SO_KEEPALIVE 启用在已连接的套接字上定期传输消息。
    // 用于检测死掉的连接或断开的客户端。
    int optval {flag ? 1 : 0};
    setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

}    // namespace ltt
