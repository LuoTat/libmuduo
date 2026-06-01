module;
#include <sys/epoll.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

Channel::Channel(int fd, EPoll* epoll): m_epoll {epoll}, m_fd {fd}
{}

void Channel::set_read_callback(REventCallback cb)
{
    m_read_cb = std::move(cb);
}

void Channel::set_write_callback(EventCallback cb)
{
    m_write_cb = std::move(cb);
}

void Channel::set_error_callback(EventCallback cb)
{
    m_error_cb = std::move(cb);
}

void Channel::add_read_event()
{
    LOG_FUNC_BEGIN();
    m_events |= (EPOLLIN | EPOLLPRI);
    update();
    LOG_INFO("channel of fd:{} has add a read event", m_fd);
    LOG_FUNC_END();
}

void Channel::del_read_event()
{
    LOG_FUNC_BEGIN();
    m_events &= ~(EPOLLIN | EPOLLPRI);
    update();
    LOG_INFO("channel of fd:{} has del a read event", m_fd);
    LOG_FUNC_END();
}

bool Channel::has_read_event() const
{
    return (m_events & (EPOLLIN | EPOLLPRI)) != 0U;
}

void Channel::add_write_event()
{
    LOG_FUNC_BEGIN();
    m_events |= EPOLLOUT;
    update();
    LOG_INFO("channel of fd:{} has add a write event", m_fd);
    LOG_FUNC_END();
}

void Channel::del_write_event()
{
    LOG_FUNC_BEGIN();
    m_events &= ~EPOLLOUT;
    update();
    LOG_INFO("channel of fd:{} has del a write event", m_fd);
    LOG_FUNC_END();
}

bool Channel::has_write_event() const
{
    return (m_events & EPOLLOUT) != 0U;
}

void Channel::del_all_event()
{
    LOG_FUNC_BEGIN();
    m_events = 0;
    update();
    LOG_INFO("channel of fd:{} has del all events", m_fd);
    LOG_FUNC_END();
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

bool Channel::is_in_epoll() const
{
    return m_in_epoll;
}

void Channel::set_in_epoll(bool flag)
{
    m_in_epoll = flag;
}

void Channel::run_event(Timestamp receive_time)
{
    LOG_FUNC_BEGIN();
    // 如果没有绑定到 TcpConnection
    // 或者 TcpConnection 还没有被销毁
    if (!m_tied || m_tcp_con.lock())
    {
        // 错误事件
        if ((m_ready_events & EPOLLERR) != 0U)
        {
            LOG_INFO("channel of fd:{} run a error event", m_fd);
            if (m_error_cb)
            {
                m_error_cb();
            }
            LOG_FUNC_END();
            return;
        }
        // 读事件
        if ((m_ready_events & (EPOLLIN | EPOLLPRI)) != 0U)
        {
            LOG_INFO("channel of fd:{} run a read event", m_fd);
            if (m_read_cb)
            {
                m_read_cb(receive_time);
            }
        }
        // 写事件
        if ((m_ready_events & EPOLLOUT) != 0U)
        {
            LOG_INFO("channel of fd:{} run a write event", m_fd);
            if (m_write_cb)
            {
                m_write_cb();
            }
        }
    }
    LOG_FUNC_END();
}

/*
 * TcpConnection 中注册了 Channel 对应的回调函数，传入的回调函数均为 TcpConnection 对象的成员方法
 * 因此可以说明一点就是：Channel 的结束一定晚于 TcpConnection 对象
 * 此处用 m_tcp_con 去持有 TcpConnection 而不增加其引用次数
 * 这样就可以判断 TcpConnection 有没有销毁
 */
void Channel::tie(const std::shared_ptr<void>& tcp_con)
{
    m_tcp_con = tcp_con;
    m_tied    = true;
}

void Channel::remove()
{
    LOG_FUNC_BEGIN();
    m_epoll->remove_channel(this);
    LOG_FUNC_END();
}

void Channel::update()
{
    LOG_FUNC_BEGIN();
    m_epoll->update_channel(this);
    LOG_FUNC_END();
}

}    // namespace ltt
