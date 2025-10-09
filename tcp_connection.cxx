module;
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

module Muduo.TCPCore;

import Muduo.Logger;

namespace ltt
{

TcpConnection::TcpConnection(int connfd, TcpServer* tcp_server, SockAddress sock_addr, SockAddress peer_addr, EventLoop* loop, std::string name):
    m_loop {loop}, m_tcp_server {tcp_server}, m_name {std::move(name)}, m_socket {std::make_unique<Socket>(connfd)}, m_channel {std::make_unique<Channel>(connfd, loop->get_epoll())}, m_sock_addr {sock_addr}, m_peer_addr {peer_addr}
{
    LOG_FUNC_BEGIN();
    m_channel->set_read_callback(
        [this](Timestamp receive_time)
        {
            LOG_FUNC_BEGIN("TcpConnection REventCallback");
            ssize_t ret {m_input_buf.read(m_channel->get_fd())};
            if (ret > 0)    // 有数据到达
            {
                m_loop->run_task(
                    [self {shared_from_this()}, receive_time]
                    {
                        self->m_readevent_cb(self, &self->m_input_buf, receive_time);
                    });
            }
            else if (ret == 0)
            {
                // 客户端发送了 FIN
                // 对端可能是半关闭状态或者直接断开连接
                close();
            }
            LOG_FUNC_END("TcpConnection REventCallback");
        });

    m_channel->set_write_callback(
        [this]
        {
            LOG_FUNC_BEGIN("TcpConnection WEventCallback");
            if (m_channel->has_write_event())
            {
                ssize_t ret {m_output_buf.write(m_channel->get_fd())};
                if (ret > 0)
                {
                    m_output_buf.retrieve(ret);
                    // 如果缓冲区空了,说明用户想发的数据全都送到内核了
                    if (!m_output_buf.get_read_size())
                    {
                        m_channel->del_write_event();
                        // 这里不能直接调用 m_writeevent_cb
                        // 因为用户的写完成回调可能又触发新的 send()
                        // 丢到任务队列,可以推迟到下一轮循环安全执行
                        m_loop->add_task(
                            [self {shared_from_this()}]
                            {
                                self->m_writeevent_cb(self);
                                LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] sent the data completely", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port());
                            });
                    }
                }
            }
            else
                LOG_ERROR("tcp connection[{}] of fd:{} from [{}] to [{}] is down, no more writing", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
            LOG_FUNC_END("TcpConnection WEventCallback");
        });

    m_channel->set_error_callback(
        [this]
        {
            LOG_FUNC_BEGIN("TcpConnection ErrorCallback");
            int       optval;
            socklen_t optlen {sizeof(optval)};
            int       err;
            if (getsockopt(m_channel->get_fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
                err = errno;
            else
                err = optval;
            LOG_ERROR("tcp connection[{}] of fd:{} from [{}] to [{}] error:{}", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port(), std::strerror(err));
            close();
            LOG_FUNC_END("TcpConnection ErrorCallback");
        });

    m_socket->set_keep_alive(true);
    LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] created", m_name, connfd, m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
    LOG_FUNC_END();
}

TcpConnection::~TcpConnection()
{
    LOG_FUNC_BEGIN();
    LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] destroyed", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
    LOG_FUNC_END();
}

EventLoop* TcpConnection::get_loop() const
{
    return m_loop;
}

std::string TcpConnection::get_name() const
{
    return m_name;
}

SockAddress TcpConnection::get_sockaddr() const
{
    return m_sock_addr;
}

SockAddress TcpConnection::get_peeraddr() const
{
    return m_peer_addr;
}

bool TcpConnection::is_connected() const
{
    return m_state == State::Connected;
}

bool TcpConnection::is_disconnecting() const
{
    return m_state == State::Disconnecting;
}

void TcpConnection::set_connection_changed_callback(ConnectionChangedCallback cb)
{
    m_connchange_cb = std::move(cb);
}

void TcpConnection::set_revent_callback(REventCallback cb)
{
    m_readevent_cb = std::move(cb);
}

void TcpConnection::set_wevent_callback(WEventCallback cb)
{
    m_writeevent_cb = std::move(cb);
}

void TcpConnection::set_high_water_mark_callback(HighWaterMarkCallback cb, std::size_t size)
{
    m_high_water_mark_cb = std::move(cb);
    m_high_water_mark    = size;
}

void TcpConnection::start_connect()
{
    LOG_FUNC_BEGIN();
    m_state = State::Connected;
    m_channel->tie(shared_from_this());
    m_channel->add_read_event();

    // 连接建立执行回调
    m_loop->run_task(
        [self {shared_from_this()}]
        {
            self->m_connchange_cb(self);
        });
    LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] started", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
    LOG_FUNC_END();
}

void TcpConnection::close()
{
    LOG_FUNC_BEGIN();
    m_channel->del_all_event();
    m_channel->remove();

    // 说明当前的连接是正在连接或半连接的状态
    if (m_state == State::Connected || m_state == State::Disconnecting)
    {
        // 连接断开执行回调
        m_loop->run_task(
            [self {shared_from_this()}]
            {
                self->m_connchange_cb(self);
                self->m_state = State::Disconnected;
                LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] closed", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port());
            });

        // 将 TcpConnection 从 TcpServer 的连接列表中删除
        m_tcp_server->m_loop->run_task(
            [self {shared_from_this()}]
            {
                self->m_tcp_server->m_connections.erase(self->m_name);
            });
    }
    else
    {
        m_state = State::Disconnected;
        // 将 TcpConnection 从 TcpServer 的连接列表中删除
        m_tcp_server->m_loop->run_task(
            [self {shared_from_this()}]
            {
                self->m_tcp_server->m_connections.erase(self->m_name);
            });
        LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] closed", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
    }

    LOG_FUNC_END();
}

void TcpConnection::send(const std::span<const std::byte>& data)
{
    LOG_FUNC_BEGIN();
    if (m_state == State::Connected || m_state == State::Disconnecting)
    {
        m_loop->run_task(
            [self {shared_from_this()}, data]
            {
                ssize_t     nwrite {0};                       // 本次 write 写入的字节数
                std::size_t remaining {data.size_bytes()};    // data 中剩余的字节数

                // 如果连接已断开，应放弃写操作
                if (self->m_state == State::Disconnected)
                {
                    LOG_ERROR("tcp connection[{}] of fd:{} from [{}] to [{}] is disconnected, give up writing", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port());
                    return;
                }

                // 如果 m_channel 第一次开始写数据
                // 且缓冲区没有待发送数据
                // 就尝试直接 write 写入数据
                if (!self->m_channel->has_write_event() && !self->m_output_buf.get_read_size())
                {
                    nwrite = write(self->m_channel->get_fd(), data.data(), remaining);
                    if (nwrite < 0)
                    {
                        nwrite = 0;
                        // 如果不是因为内核缓冲区写满而导致的 EAGAIN 错误
                        // 则需要记录错误日志
                        if (errno != EAGAIN)
                        {
                            LOG_ERROR("write() failed! error:{}", std::strerror(errno));
                            // 如果对端断开或管道错误,则直接返回
                            if (errno == ECONNRESET || errno == EPIPE)
                                return;
                        }
                    }
                    else
                    {
                        remaining -= nwrite;
                        // 如果数据全部发送完成，
                        // 就不用再给 m_channel 设置写事件
                        if (!remaining)
                        {
                            // 直接执行发送完成的回调
                            self->m_loop->add_task(
                                [self]
                                {
                                    self->m_writeevent_cb(self);
                                });
                            LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] sent the data completely", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port());
                            return;
                        }
                    }
                }

                /*
                 * 说明当前这一次 write 并没有把数据全部发送出去,剩余的数据需要保存到缓冲区当中
                 * 然后给 m_channel 注册 EPOLLOUT 事件
                 */
                if (remaining > 0)
                {
                    // 目前发送缓冲区剩余的待发送的数据的长度
                    size_t read_size {self->m_output_buf.get_read_size()};
                    if (read_size < self->m_high_water_mark && read_size + remaining >= self->m_high_water_mark && self->m_high_water_mark_cb)
                    {
                        self->m_loop->add_task(
                            [self, read_size, remaining]
                            {
                                self->m_high_water_mark_cb(self, read_size + remaining);
                            });
                    }
                    self->m_output_buf.append(data.subspan(nwrite, remaining));
                    if (!self->m_channel->has_write_event())
                        self->m_channel->add_write_event();    // 这里一定要注册 m_channel 的写事件
                    LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] has {} bytes data remaining in output buffer", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port(), self->m_output_buf.get_read_size());
                }
            });
    }
    else
        LOG_ERROR("tcp connection[{}] of fd:{} from [{}] to [{}] is connecting/disconnected, give up sending", m_name, m_channel->get_fd(), m_peer_addr.get_ip_with_port(), m_sock_addr.get_ip_with_port());
    LOG_FUNC_END();
}

void TcpConnection::shutdown()
{
    LOG_FUNC_BEGIN();

    if (m_state == State::Connected)
    {
        m_state = State::Disconnecting;
        if (!m_channel->has_write_event())    // 说明当前 m_output_buf 的数据全部向外发送完成
            m_loop->run_task(
                [self {shared_from_this()}]
                {
                    self->m_socket->shutdown_write();
                    LOG_INFO("tcp connection[{}] of fd:{} from [{}] to [{}] is shutdown", self->m_name, self->m_channel->get_fd(), self->m_peer_addr.get_ip_with_port(), self->m_sock_addr.get_ip_with_port());
                });
    }
    LOG_FUNC_END();
}

}    // namespace ltt