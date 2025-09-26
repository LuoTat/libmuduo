module;
#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

// 防止一个线程创建多个 EventLoop
static thread_local EventLoop* st_loop_of_this_thread {nullptr};

EventLoop::EventLoop():
    m_id {EventLoop::m_next_id++}, m_epoll {std::make_unique<EPoll>(this)}
{
    LOG_DEBUG("EventLoop:{} created in thread:{}", m_id, m_thread_id);
    if (st_loop_of_this_thread)
        LOG_FATAL("EventLoop:{} exists in thread:{}", st_loop_of_this_thread->m_id, m_thread_id);
    else
        st_loop_of_this_thread = this;

    m_wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_wakeup_fd < 0)
        LOG_FATAL("eventfd failed! error:{}", std::strerror(errno));

    m_wakeup_channel = std::make_unique<Channel>(m_epoll.get(), m_wakeup_fd);
    m_wakeup_channel->set_read_callback(
        [this](Timestamp)
        {
            if (eventfd_read(m_wakeup_fd, nullptr))
                LOG_ERROR("EventLoop:{} eventfd_read failed! error:{}", m_id, std::strerror(errno));
        });

    m_wakeup_channel->add_read_event();
}

EventLoop::~EventLoop()
{
    m_wakeup_channel->del_all_event();
    m_wakeup_channel->remove();
    close(m_wakeup_fd);
    st_loop_of_this_thread = nullptr;
}

void EventLoop::loop()
{
    m_looping = true;
    m_quit    = false;
    LOG_INFO("EventLoop:{} start looping", m_id);

    while (!m_quit)
    {
        m_ready_channels.clear();
        using namespace std::chrono_literals;
        m_poll_reture_tp = m_epoll->poll(10s, m_ready_channels);
        for (auto channel : m_ready_channels)
            channel->run_event(m_poll_reture_tp);

        // epoll 里面的任务完成
        // 执行提交给 loop 的任务
        run_all_tasks();
    }

    LOG_INFO("EventLoop:{} stop looping.", m_id);
    m_looping = false;
}

void EventLoop::quit()
{
    m_quit = true;

    // 如果不是在当前 EventLoop 所属的线程中调用 quit
    // 则需要唤醒 EventLoop 所属线程的 epoll_wait
    // 通过向 epoll 里面添加一个 wakeup_fd，从而达到让 epoll_wait 立即退出
    // 否则，可能要等待 10s 才能退出
    if (!is_in_loop_thread())
        wakeup();
}

void EventLoop::wakeup()
{
    if (eventfd_write(m_wakeup_fd, 1))
        LOG_ERROR("EventLoop:{} eventfd_write failed! error:{}", m_id, std::strerror(errno));
}

void EventLoop::add_task(Task task)
{
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        m_tasks.emplace_back(task);
    }

    // || m_running_task 的意思是提前唤醒，可以节约时间
    if (!is_in_loop_thread() || m_running_task)
        wakeup();
}

void EventLoop::run_task(Task task)
{
    if (is_in_loop_thread())
        task();
    else
        add_task(task);
}

void EventLoop::update_channel(Channel* channel)
{
    m_epoll->update_channel(channel);
}

void EventLoop::remove_channel(Channel* channel)
{
    m_epoll->remove_channel(channel);
}

bool EventLoop::has_channel(Channel* channel)
{
    return m_epoll->has_channel(channel);
}

bool EventLoop::is_in_loop_thread() const
{
    return m_thread_id == std::this_thread::get_id();
}

Timestamp EventLoop::get_poll_return_tp() const
{
    return m_poll_reture_tp;
}

void EventLoop::run_all_tasks()
{
    std::vector<Task> tasks;
    m_running_task = true;

    {
        std::unique_lock<std::mutex> lock {m_mutex};

        // 交换的方式减少了锁的临界区范围提升效率
        // 同时避免了死锁, 如果在临界区内执行 task()  且 task() 中调用了 add_task() 就会产生死锁
        tasks.swap(m_tasks);
    }

    for (auto& task : tasks)
        task();

    m_running_task = false;
}

}    // namespace ltt