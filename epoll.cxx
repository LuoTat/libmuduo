module;
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

EPoll::EPoll(EventLoop* eventloop):
    m_eventloop {eventloop}, m_epoll_fd {epoll_create1(EPOLL_CLOEXEC)}
{
    if (m_epoll_fd < 0)
        LOG_FATAL("epoll_create1 failed! error:{}", std::strerror(errno));
    m_epoll_events.resize(16);
}

EPoll::~EPoll()
{
    close(m_epoll_fd);
}

Timestamp EPoll::poll(std::chrono::milliseconds ms, std::span<Channel*> ready_channels)
{
    LOG_DEBUG("start polling");

    int  ret {epoll_wait(m_epoll_fd, m_epoll_events.data(), m_epoll_events.size(), ms.count())};
    int  saved_errno {errno};
    auto tp {Timestamp::now()};

    if (ret > 0)
    {
        LOG_DEBUG("{} fd has ready events", ret);
        for (int i {0}; i < ret; ++i)
        {
            Channel* channel = static_cast<Channel*>(m_epoll_events[i].data.ptr);
            channel->set_ready_events(m_epoll_events[i].events);
            ready_channels[i] = channel;
        }
        if (std::cmp_equal(ret, m_epoll_events.size()))    // 扩容操作
            m_epoll_events.resize(m_epoll_events.size() * 2);
    }
    else if (ret == 0)
        LOG_DEBUG("epoll_wait has timeout");
    else
    {
        if (saved_errno != EINTR)
        {
            errno = saved_errno;
            LOG_ERROR("epoll_wait failed! error:{}", std::strerror(errno));
        }
    }
    LOG_DEBUG("stop polling");
    return tp;
}

void EPoll::update_channel(Channel* channel)
{
    LOG_DEBUG("update channel of fd:{}", channel->get_fd());

    bool in_map {m_channels_map.contains(channel->get_fd())};
    bool in_epoll {channel->get_in_epoll()};

    if (in_epoll)
    {
        if (channel->has_events())
            update_epoll(channel, EPOLL_CTL_MOD);
        else
        {
            update_epoll(channel, EPOLL_CTL_DEL);
            channel->set_in_epoll(false);
        }
    }
    else
    {
        if (!in_map)
            m_channels_map.emplace(channel->get_fd(), channel);
        update_epoll(channel, EPOLL_CTL_ADD);
        channel->set_in_epoll(true);
    }
}

void EPoll::remove_channel(Channel* channel)
{
    LOG_DEBUG("remove channel of fd:{}", channel->get_fd());

    auto it {m_channels_map.find(channel->get_fd())};
    if (it != m_channels_map.end())
    {
        m_channels_map.erase(it);
        update_epoll(channel, EPOLL_CTL_DEL);
        channel->set_in_epoll(false);
    }
}

bool EPoll::has_channel(Channel* channel) const
{
    auto it {m_channels_map.find(channel->get_fd())};
    return it != m_channels_map.end() && it->second == channel;
}

void EPoll::update_epoll(Channel* channel, int flag)
{
    epoll_event event;
    int         fd {channel->get_fd()};

    event.events   = channel->get_events();
    event.data.ptr = channel;

    if (epoll_ctl(m_epoll_fd, flag, fd, &event) < 0)
    {
        if (flag == EPOLL_CTL_DEL)
            LOG_ERROR("epoll_ctl del failed! error:{}", std::strerror(errno));
        else
            LOG_FATAL("epoll_ctl add/mod failed! error:{}", std::strerror(errno));
    }
}

}    // namespace ltt