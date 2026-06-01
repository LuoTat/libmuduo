module;
#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

namespace
{

std::string get_ready_events_string(std::uint32_t events)
{
    std::vector<std::string> parts;

    if ((events & EPOLLERR) != 0U)
    {
        parts.emplace_back("error");
    }
    if ((events & (EPOLLIN | EPOLLPRI)) != 0U)
    {
        parts.emplace_back("read");
    }
    if ((events & EPOLLOUT) != 0U)
    {
        parts.emplace_back("write");
    }

    // 用 join 拼接
    std::string result;
    for (auto&& str : parts | std::views::join_with(std::string("/")))
    {
        result.push_back(str);
    }

    return result;
}

}    // namespace

EPoll::EPoll(EventLoop* loop): m_loop {loop}, m_epoll_fd {epoll_create1(EPOLL_CLOEXEC)}
{
    LOG_FUNC_BEGIN();
    if (m_epoll_fd < 0)
    {
        LOG_FATAL("epoll_create1() failed! error:{}", std::strerror(errno));
    }
    m_epoll_events.resize(16);
    LOG_FUNC_END();
}

EPoll::~EPoll()
{
    LOG_FUNC_BEGIN();
    close(m_epoll_fd);
    LOG_FUNC_END();
}

Timestamp EPoll::poll(std::chrono::milliseconds ms, std::vector<Channel*>& ready_channels)
{
    LOG_FUNC_BEGIN();
    int  ret {epoll_wait(
        m_epoll_fd, m_epoll_events.data(), static_cast<int>(m_epoll_events.size()), static_cast<int>(ms.count())
    )};
    int  saved_errno {errno};
    auto tp {Timestamp::now()};

    if (ret > 0)
    {
        LOG_INFO("{} fds has ready events", ret);
        for (int i : std::views::iota(0, ret))
        {
            auto* channel = static_cast<Channel*>(m_epoll_events[i].data.ptr);
            channel->set_ready_events(m_epoll_events[i].events);
            LOG_INFO(
                "channel of fd:{} has ready events:{}",
                channel->get_fd(),
                get_ready_events_string(m_epoll_events[i].events)
            );
            ready_channels.push_back(channel);
        }
        if (std::cmp_equal(ret, m_epoll_events.size()))
        {    // 扩容操作
            m_epoll_events.resize(m_epoll_events.size() * 2);
        }
    }
    else if (ret == 0)
    {
        LOG_INFO("epoll_wait() has timeout");
    }
    else
    {
        if (saved_errno != EINTR)
        {
            errno = saved_errno;
            LOG_ERROR("epoll_wait() failed! error:{}", std::strerror(errno));
        }
    }
    LOG_FUNC_END();
    return tp;
}

void EPoll::update_channel(Channel* channel)
{
    LOG_FUNC_BEGIN();
    if (channel->is_in_epoll())
    {
        if (channel->has_events())
        {
            update_epoll(channel, EPOLL_CTL_MOD);
        }
        else
        {
            m_channels_map.erase(channel->get_fd());
            update_epoll(channel, EPOLL_CTL_DEL);
            channel->set_in_epoll(false);
        }
    }
    else
    {
        m_channels_map.emplace(channel->get_fd(), channel);
        update_epoll(channel, EPOLL_CTL_ADD);
        channel->set_in_epoll(true);
    }
    LOG_FUNC_END();
}

void EPoll::remove_channel(Channel* channel)
{
    LOG_FUNC_BEGIN();
    auto it {m_channels_map.find(channel->get_fd())};
    if (it != m_channels_map.end())
    {
        m_channels_map.erase(it);
        update_epoll(channel, EPOLL_CTL_DEL);
        channel->set_in_epoll(false);
    }
    LOG_FUNC_END();
}

bool EPoll::has_channel(Channel* channel) const
{
    auto it {m_channels_map.find(channel->get_fd())};
    return it != m_channels_map.end() && it->second == channel;
}

void EPoll::update_epoll(Channel* channel, int flag) const
{
    LOG_FUNC_BEGIN();
    epoll_event event {};
    int         fd {channel->get_fd()};

    event.events   = channel->get_events();
    event.data.ptr = channel;

    if (epoll_ctl(m_epoll_fd, flag, fd, &event) < 0)
    {
        if (flag == EPOLL_CTL_ADD)
        {
            LOG_FATAL("epoll_ctl() add failed! error:{}", std::strerror(errno));
        }
        else if (flag == EPOLL_CTL_MOD)
        {
            LOG_FATAL("epoll_ctl() mod failed! error:{}", std::strerror(errno));
        }
        else
        {
            LOG_ERROR("epoll_ctl() del failed! error:{}", std::strerror(errno));
        }
    }
    LOG_FUNC_END();
}

}    // namespace ltt
