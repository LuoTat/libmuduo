module Muduo.EventLoopThreadPool;

import Muduo.Logger;

namespace ltt
{

EventLoopThreadPool::EventLoopThreadPool(EventLoop* loop, std::string name):
    m_base_loop {loop}, m_name {std::move(name)}
{}

EventLoopThreadPool::~EventLoopThreadPool()
{}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    LOG_FUNC_BEGIN();
    m_started = true;
    for (int i : std::views::iota(0, m_threads_num))
    {
        m_threads.push_back(std::make_unique<EventLoopThread>(cb, std::format("{}#{}", m_name, i)));
        m_loops.push_back(m_threads.back()->start_loop());
    }

    if ((m_threads_num == 0) && cb)
    {    // 整个服务端只有一个线程运行 m_base_loop
        cb(m_base_loop);
    }

    LOG_FUNC_END();
}

void EventLoopThreadPool::set_thread_num(int num)
{
    m_threads_num = num;
}

EventLoop* EventLoopThreadPool::get_next_loop()
{
    // 如果只设置一个线程
    // 则每次都返回当前的 m_base_loop
    EventLoop* next_loop {m_base_loop};

    // 通过轮询获取下一个处理事件的loop
    if (!m_loops.empty())
    {
        next_loop = m_loops[m_next_id++];

        // 轮询
        if (m_next_id >= m_loops.size())
        {
            m_next_id = 0;
        }
    }

    return next_loop;
}

std::vector<EventLoop*> EventLoopThreadPool::get_all_loops() const
{
    if (m_loops.empty())
    {
        return std::vector {m_base_loop};
    }
    return m_loops;
}

std::string EventLoopThreadPool::get_name() const
{
    return m_name;
}

bool EventLoopThreadPool::is_started() const
{
    return m_started;
}

}    // namespace ltt
