module Muduo.Timestamp;

namespace ltt
{

Timestamp::Timestamp(): m_time_point {std::chrono::system_clock::time_point::min()}
{}

Timestamp::Timestamp(std::chrono::system_clock::time_point time_point): m_time_point {time_point}
{}

std::string Timestamp::to_string() const
{
    // 构造本地时区的 zoned_time
    std::chrono::zoned_time local {std::chrono::current_zone(), m_time_point};
    return std::format("{:%Y-%m-%d %H:%M:%S}", floor<std::chrono::seconds>(local.get_local_time()));
}

Timestamp Timestamp::now()
{
    return Timestamp {std::chrono::system_clock::now()};
}

}    // namespace ltt
