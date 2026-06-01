module;
#include <arpa/inet.h>
#include <cerrno>

module Muduo.SockAddress;

import Muduo.Logger;

namespace ltt
{

SockAddress::SockAddress(std::string ip, std::uint16_t port)
{
    std::memset(&m_sock_addr, 0, sizeof(m_sock_addr));
    m_sock_addr.sin_family = AF_INET;
    m_sock_addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &m_sock_addr.sin_addr) == 0)
    {
        LOG_ERROR("inet_pton() failed! Invalid IP string format:{}", ip);
    }
}

SockAddress::SockAddress(sockaddr_in addr): m_sock_addr(addr)
{}

std::string SockAddress::get_ip() const
{
    std::string ip;
    ip.resize(INET_ADDRSTRLEN);
    if (inet_ntop(AF_INET, &m_sock_addr.sin_addr, ip.data(), ip.size()) == nullptr)
    {
        LOG_ERROR("inet_ntop() failed! error:{}", std::strerror(errno));
        return {};
    }

    // 去除多余的 '\0' 以便 size 和字符串的实际大小一致
    ip.resize(std::strlen(ip.c_str()));
    return ip;
}

std::string SockAddress::get_ip_with_port() const
{
    std::string ip;
    ip.resize(INET_ADDRSTRLEN);
    if (inet_ntop(AF_INET, &m_sock_addr.sin_addr, ip.data(), ip.size()) == nullptr)
    {
        LOG_ERROR("inet_ntop() failed! error:{}", std::strerror(errno));
        return {};
    }

    // 去除多余的 '\0' 以便 size 和字符串的实际大小一致
    ip.resize(std::strlen(ip.c_str()));
    uint16_t port = ntohs(m_sock_addr.sin_port);
    return std::format("{}:{}", ip, port);
}

std::uint16_t SockAddress::get_port() const
{
    return ntohs(m_sock_addr.sin_port);
}

sockaddr_in SockAddress::get_sock_addr() const
{
    return m_sock_addr;
}

void SockAddress::set_sock_addr(sockaddr_in addr)
{
    m_sock_addr = addr;
}

}    // namespace ltt
