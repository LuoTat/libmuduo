module;
#include <cerrno>
#include <sys/eventfd.h>
#include <unistd.h>

module Muduo.Core;

import Muduo.Logger;

namespace ltt
{

static std::atomic_int16_t s_next_id {0};
// 防止一个线程创建多个 EventLoop
static thread_local EventLoop* st_loop_of_this_thread {nullptr};

EventLoop::EventLoop():
    m_id {s_next_id++}, m_epoll {std::make_unique<EPoll>(this)}
{
    LOG_FUNC_BEGIN();
    LOG_INFO("EventLoop[{}] created in thread[{}]", m_id, m_thread_id);
    if (st_loop_of_this_thread)
        LOG_FATAL("EventLoop[{}] exists in thread[{}]", st_loop_of_this_thread->m_id, m_thread_id);
    else
        st_loop_of_this_thread = this;

    m_wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_wakeup_fd < 0)
        LOG_FATAL("eventfd() failed! error:{}", std::strerror(errno));

    m_wakeup_channel = std::make_unique<Channel>(m_wakeup_fd, m_epoll.get());
    m_wakeup_channel->set_read_callback(
        [this](Timestamp)
        {
            LOG_FUNC_BEGIN("EventLoop REventCallback");
            eventfd_t one;
            if (eventfd_read(m_wakeup_fd, &one))
                LOG_ERROR("EventLoop[{}] eventfd_read() failed! error:{}", m_id, std::strerror(errno));
            LOG_FUNC_END("EventLoop REventCallback");
        });

    m_wakeup_channel->add_read_event();
    LOG_FUNC_END();
}

EventLoop::~EventLoop()
{
    LOG_FUNC_BEGIN();
    m_wakeup_channel->del_all_event();
    m_wakeup_channel->remove();
    close(m_wakeup_fd);
    st_loop_of_this_thread = nullptr;
    LOG_FUNC_END();
}

void EventLoop::loop()
{
    LOG_FUNC_BEGIN();
    m_looping = true;
    m_quit    = false;
    LOG_INFO("EventLoop[{}] start looping", m_id);

    while (!m_quit)
    {
        m_ready_channels.clear();
        using namespace std::chrono_literals;
        m_poll_return_tp = m_epoll->poll(10s, m_ready_channels);
        for (auto channel : m_ready_channels)
            channel->run_event(m_poll_return_tp);

        // epoll 里面的任务完成
        // 执行提交给 loop 的任务
        run_all_tasks();
    }

    LOG_INFO("EventLoop[{}] stop looping.", m_id);
    m_looping = false;
    LOG_FUNC_END();
}

void EventLoop::quit()
{
    LOG_FUNC_BEGIN();
    m_quit = true;

    // 如果不是在当前 EventLoop 所属的线程中调用 quit
    // 则需要唤醒 EventLoop 所属线程的 epoll_wait
    // 通过向 epoll 里面添加一个 wakeup_fd，从而达到让 epoll_wait 立即退出
    // 否则，可能要等待 10s 才能退出
    if (!is_in_loop_thread())
        wakeup();
    LOG_FUNC_END();
}

void EventLoop::wakeup()
{
    LOG_FUNC_BEGIN();
    if (eventfd_write(m_wakeup_fd, 1))
        LOG_ERROR("EventLoop[{}] eventfd_write() failed! error:{}", m_id, std::strerror(errno));
    LOG_FUNC_END();
}

void EventLoop::run_task(Task task)
{
    LOG_FUNC_BEGIN();
    if (is_in_loop_thread())
        task();
    else
        add_task(task);
    LOG_FUNC_END();
}

void EventLoop::add_task(Task task)
{
    LOG_FUNC_BEGIN();
    {
        std::unique_lock<std::mutex> lock {m_mutex};
        m_tasks.emplace_back(task);
    }

    // || m_running_task 的意思是提前唤醒，可以节约时间
    if (!is_in_loop_thread() || m_running_task)
        wakeup();
    LOG_FUNC_END();
}

EPoll* EventLoop::get_epoll() const
{
    return m_epoll.get();
}

int16_t EventLoop::get_id() const
{
    return m_id;
}

std::thread::id EventLoop::get_thread_id() const
{
    return m_thread_id;
}

bool EventLoop::is_in_loop_thread() const
{
    return m_thread_id == std::this_thread::get_id();
}

Timestamp EventLoop::get_poll_return_tp() const
{
    return m_poll_return_tp;
}

void EventLoop::run_all_tasks()
{
    LOG_FUNC_BEGIN();
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
    LOG_FUNC_END();
}

}    // namespace ltt