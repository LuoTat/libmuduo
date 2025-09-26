module;
#include <sys/epoll.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

Channel::Channel(EPoll* epoll, int fd):
    m_epoll {epoll}, m_fd {fd}
{}

void Channel::set_read_callback(ReadEventCallback cb)
{
    m_read_callback = std::move(cb);
}

void Channel::set_write_callback(EventCallback cb)
{
    m_write_callback = std::move(cb);
}

void Channel::set_close_callback(EventCallback cb)
{
    m_close_callback = std::move(cb);
}

void Channel::set_error_callback(EventCallback cb)
{
    m_error_callback = std::move(cb);
}

void Channel::add_read_event()
{
    m_events |= (EPOLLIN | EPOLLPRI);
    update();
}

void Channel::del_read_event()
{
    m_events &= ~(EPOLLIN | EPOLLPRI);
    update();
}

bool Channel::has_read_event() const
{
    return m_events & (EPOLLIN | EPOLLPRI);
}

void Channel::add_write_event()
{
    m_events |= EPOLLOUT;
    update();
}

void Channel::del_write_event()
{
    m_events &= ~EPOLLOUT;
    update();
}

bool Channel::has_write_event() const
{
    return m_events & EPOLLOUT;
}

void Channel::del_all_event()
{
    m_events = 0;
    update();
}

bool Channel::has_events() const
{
    return m_events != 0;
}

EPoll* Channel::get_epoll() const
{
    return m_epoll;
}

int Channel::get_fd() const
{
    return m_fd;
}

std::uint32_t Channel::get_events() const
{
    return m_events;
}

void Channel::set_ready_events(std::uint32_t ready_events)
{
    m_ready_events = ready_events;
}

bool Channel::get_in_epoll() const
{
    return m_in_epoll;
}

void Channel::set_in_epoll(bool flag)
{
    m_in_epoll = flag;
}

void Channel::run_event(Timestamp receive_time)
{
    // 如果没有绑定到 TcpConnection
    // 或者 TcpConnection 还没有被销毁
    if (!m_tied || m_tcp_con.lock())
    {
        LOG_DEBUG("channel of fd:{} runs the events:{}", m_fd, m_ready_events);

        // 关闭事件
        // 当 TcpConnection 对应 Channel 通过 shutdown 关闭写端从而触发 EPOLLHUP
        // 并且没有可读数据
        if ((m_ready_events & EPOLLHUP) && !(m_ready_events & EPOLLIN))
        {
            if (m_close_callback)
                m_close_callback();
        }
        // 错误事件
        if (m_ready_events & EPOLLERR)
        {
            if (m_error_callback)
                m_error_callback();
        }
        // 读事件
        if (m_ready_events & (EPOLLIN | EPOLLPRI))
        {
            if (m_read_callback)
                m_read_callback(receive_time);
        }
        // 写事件
        if (m_ready_events & EPOLLOUT)
        {
            if (m_write_callback)
                m_write_callback();
        }
    }
}

/*
 * TcpConnection 中注册了 Channel 对应的回调函数，传入的回调函数均为 TcpConnection 对象的成员方法
 * 因此可以说明一点就是：Channel 的结束一定晚于 TcpConnection 对象
 * 此处用 m_tcp_con 去持有 TcpConnection 而不增加其引用次数
 * 这样就可以判断 TcpConnection 有没有销毁
 */
void Channel::tie(const std::shared_ptr<TcpConnection>& tcp_con)
{
    m_tcp_con = tcp_con;
    m_tied    = true;
}

void Channel::remove()
{
    m_epoll->remove_channel(this);
}

void Channel::update()
{
    m_epoll->update_channel(this);
}

}    // namespace ltt