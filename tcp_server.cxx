module;
#include <cerrno>
#include <netinet/in.h>

module Muduo.TCPCore;

import Muduo.Logger;

namespace ltt
{

TcpServer::TcpServer(EventLoop* loop, SockAddress addr, std::string name):
    m_loop {loop}, m_ip_with_port {addr.get_ip_with_port()}, m_name {std::move(name)},
    m_acceptor {std::make_unique<Acceptor>(loop, addr)},
    m_thread_pool {std::make_shared<EventLoopThreadPool>(loop, name)}
{
    LOG_FUNC_BEGIN();
    // 给 m_acceptor 注册一个回调
    // 一旦有新连接，调用 TcpServer::new_conn_cb
    m_acceptor->set_new_conn_callback(
        [this](int connfd, SockAddress peer_addr) -> void
        {
            LOG_FUNC_BEGIN("TcpServer NewConnectionCallback");
            // 轮询算法 选择一个 subloop 来管理 connfd 对应的 channel
            EventLoop* subloop {m_thread_pool->get_next_loop()};

            // 这里没有设置 m_next_conn_id 为原子类是因为其只在 mainloop 中执行
            // 不涉及线程安全问题
            std::string conn_name {std::format("{}_{}#{}", m_name, m_ip_with_port, m_next_conn_id++)};

            // 通过 connfd 获取其绑定的本机的具体的 ip 地址和端口信息
            // 因为服务器可能绑定了多个 ip 地址
            sockaddr_in local_addr {};
            socklen_t   addrlen {sizeof(local_addr)};

            if (getsockname(connfd, std::bit_cast<sockaddr*>(&local_addr), &addrlen))
            {
                LOG_ERROR("getsockname() failed! error:{}", std::strerror(errno));
            }

            auto conn_tcp {
                std::make_shared<TcpConnection>(connfd, this, SockAddress(local_addr), peer_addr, subloop, conn_name)
            };
            m_connections.emplace(conn_tcp->get_name(), conn_tcp);

            // 下面的回调都是用户设置给 TcpServer
            // 而后 TcpServer 又设置给 TcpConnection
            conn_tcp->set_connection_changed_callback(m_connchange_cb);
            conn_tcp->set_revent_callback(m_readevent_cb);
            conn_tcp->set_wevent_callback(m_writeevent_cb);

            subloop->run_task(
                [conn_tcp] -> void
                {
                    conn_tcp->start_connect();
                }
            );

            LOG_FUNC_END("TcpServer NewConnectionCallback");
        }
    );
    LOG_FUNC_END();
}

TcpServer::~TcpServer()
{
    LOG_FUNC_BEGIN();
    for (auto& item : m_connections)
    {
        // 用 std::move 转移所有权
        TcpConnectionPtr conn_tcp {std::move(item.second)};

        // 销毁连接
        conn_tcp->get_loop()->run_task(
            [conn_tcp] -> void
            {
                conn_tcp->shutdown();
            }
        );
    }
    LOG_FUNC_END();
}

void TcpServer::set_thread_init_callback(ThreadInitCallback cb)
{
    m_threadinit_cb = std::move(cb);
}

void TcpServer::set_connection_changed_callback(ConnectionChangedCallback cb)
{
    m_connchange_cb = std::move(cb);
}

void TcpServer::set_revent_callback(REventCallback cb)
{
    m_readevent_cb = std::move(cb);
}

void TcpServer::set_wevent_callback(WEventCallback cb)
{
    m_writeevent_cb = std::move(cb);
}

void TcpServer::set_thread_num(int num)
{
    m_thread_num = num;
    m_thread_pool->set_thread_num(num);
}

void TcpServer::start()
{
    LOG_FUNC_BEGIN();
    std::call_once(
        m_start_flag,
        [this] -> void
        {
            m_thread_pool->start(m_threadinit_cb);
            m_loop->run_task(
                [this] -> void
                {
                    m_acceptor->listen();
                }
            );
        }
    );
    LOG_FUNC_END();
}

}    // namespace ltt
