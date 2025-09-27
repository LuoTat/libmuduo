module;
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>

module Muduo.Buffer;

import Muduo.Logger;

namespace ltt
{

Buffer::Buffer(std::size_t size):
    m_buffer(Prepend_Size + size), m_reader_index(Prepend_Size), m_writer_index(Prepend_Size)
{
}

std::size_t Buffer::get_read_size() const
{
    return m_writer_index - m_reader_index;
}

std::size_t Buffer::get_write_size() const
{
    return m_buffer.size() - m_writer_index;
}

std::size_t Buffer::get_unused_size() const
{
    return m_reader_index;
}

const std::byte* Buffer::peek() const
{
    return m_buffer.data() + m_reader_index;
}

void Buffer::retrieve(std::size_t size)
{
    if (size < get_read_size())
        m_reader_index += size;
    else
        retrieve_all();
}

void Buffer::retrieve_all()
{
    m_reader_index = Prepend_Size;
    m_writer_index = Prepend_Size;
}

std::string Buffer::retrieve_as_str(std::size_t size)
{
    std::string str {reinterpret_cast<const char*>(peek()), size};
    // 上面把缓冲区中可读的数据已经读取出来 这里对缓冲区进行复位操作
    retrieve(size);
    return str;
}

std::string Buffer::retrieve_all_as_str()
{
    return retrieve_as_str(get_read_size());
}

void Buffer::ensure_writable_size(std::size_t size)
{
    if (size > get_write_size())
        resize_writable_size(size);    // 扩容
}

void Buffer::append(const std::byte* data, std::size_t size)
{
    ensure_writable_size(size);
    std::copy(data, data + size, m_buffer.data() + m_writer_index);
    m_writer_index += size;
}

ssize_t Buffer::read(int fd)
{
    // 栈额外空间，用于从套接字往出读时，当 m_buffer 暂时不够用时暂存数据，
    // 待 m_buffer 重新分配足够空间后，再把数据交换给 m_buffer
    std::byte buf[65536];

    // 使用 iovec 分配两个连续的缓冲区
    iovec vec[2];

    // 这是 m_buffer 缓冲区剩余的可写空间大小
    // 不一定能完全存储从 fd 读出的数据
    size_t write_size {get_write_size()};

    // 第一块缓冲区,指向可写空间
    vec[0].iov_base = m_buffer.data() + m_writer_index;
    vec[0].iov_len  = write_size;
    // 第二块缓冲区,指向栈空间
    vec[1].iov_base = buf;
    vec[1].iov_len  = sizeof(buf);

    // 如果 m_buffer 里面的 write_size >= 64KB, 则只用 m_buffer
    // 否则才需要 buf 的额外空间
    ssize_t ret {readv(fd, vec, (write_size >= sizeof(buf)) ? 1 : 2)};

    if (ret < 0)
        LOG_ERROR("readv failed! error:{}", std::strerror(errno));
    // m_buffer 够存储读出来的数据
    else if (std::cmp_less_equal(ret, write_size))
        m_writer_index += ret;
    else
    {
        m_writer_index = m_buffer.size();
        append(buf, ret - write_size);
    }
    return ret;
}

ssize_t Buffer::write(int fd)
{
    ssize_t ret {::write(fd, peek(), get_read_size())};
    if (ret < 0)
        LOG_ERROR("write failed! error:{}", std::strerror(errno));
    return ret;
}

void Buffer::resize_writable_size(std::size_t size)
{
    if (get_write_size() + get_unused_size() < size + Prepend_Size)
    {
        m_buffer.resize(m_writer_index + size);
    }
    else
    {
        std::size_t read_size {get_read_size()};
        std::copy(m_buffer.data() + m_reader_index, m_buffer.data() + m_writer_index, m_buffer.data() + Prepend_Size);
        m_reader_index = Prepend_Size;
        m_writer_index = m_reader_index + read_size;
    }
}

}    // namespace ltt