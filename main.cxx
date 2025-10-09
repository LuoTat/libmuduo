import std;
import Muduo;

using namespace ltt;

// 构建HTTP响应
constexpr char response[] = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 13\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "Hello, World!";

class LTTServer
{
public:
    LTTServer(EventLoop* loop, SockAddress addr, std::string name):
        m_server {loop, addr, name}
    {
        // 注册回调函数
        m_server.set_thread_init_callback(
            [](EventLoop* loop)
            {
                LOG_INFO("subloop thread[{}] is inited", loop->get_thread_id());
            });

        m_server.set_connection_changed_callback(
            [](const TcpConnectionPtr&) {});

        m_server.set_revent_callback(
            [](const TcpConnectionPtr& conn_tcp, Buffer* buf, Timestamp)
            {
                buf->retrieve_all();
                conn_tcp->send(std::as_bytes(std::span {response, std::strlen(response)}));
                conn_tcp->shutdown();
            });

        m_server.set_wevent_callback(
            [](const TcpConnectionPtr&) {});

        m_server.set_thread_num(16);
    }

    void start()
    {
        m_server.start();
    }

private:
    TcpServer m_server;
};

int main()
{
    EventLoop   loop;
    SockAddress addr {"127.0.0.1", 8080};
    LTTServer   server(&loop, addr, "LTTServer");
    server.start();
    loop.loop();
    return 0;
}