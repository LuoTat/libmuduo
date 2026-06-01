module Muduo.EventLoopThread;

import Muduo.Logger;

namespace ltt
{

EventLoopThread::EventLoopThread(ThreadInitCallback cb, std::string name):
    m_name {std::move(name)}, m_thrinit_cb {std::move(cb)}
{}

EventLoopThread::~EventLoopThread()
{
    LOG_FUNC_BEGIN();
    m_exiting = true;
    if (m_loop != nullptr)
    {
        m_loop->quit();
        m_thread.join();
    }
    LOG_FUNC_END();
}

EventLoop* EventLoopThread::start_loop()
{
    LOG_FUNC_BEGIN();
    std::promise<EventLoop*> promise;
    std::future<EventLoop*>  future {promise.get_future()};

    m_thread = std::jthread(
        [this, p {std::move(promise)}] mutable -> void
        {
            LOG_FUNC_BEGIN("EventLoopThread main function");
            // 在新线程中创建 EventLoop 对象
            // 遵循 one loop per thread 原则
            EventLoop loop;

            // 如果设置了初始化回调函数，则调用它
            // 可以在事件循环启动前执行一些自定义初始化操作
            if (m_thrinit_cb)
            {
                m_thrinit_cb(&loop);
            }

            p.set_value(&loop);

            LOG_INFO("EventLoopThread[{}] start looping", m_name);
            loop.loop();
            LOG_INFO("EventLoopThread[{}] stop looping", m_name);
            m_loop = nullptr;
            LOG_FUNC_END("EventLoopThread main function");
        }
    );

    // 主线程阻塞等待，直到子线程创建好 EventLoop
    LOG_FUNC_END();
    return future.get();
}

std::string EventLoopThread::get_name() const
{
    return m_name;
}

}    // namespace ltt
