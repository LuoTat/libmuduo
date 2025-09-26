module Muduo.Logger;

import Muduo.Timestamp;

namespace ltt
{

void Logger::set_loglevel(LogLevel loglevel)
{
    m_loglevel = loglevel;
}

void Logger::log(std::string_view msg)
{
    switch (m_loglevel)
    {
        case LogLevel::INFO :
            std::println("{} [INFO] :{}", Timestamp::now().toString(), msg);
            break;
        case LogLevel::ERROR :
            std::println("{} [ERROR]:{}", Timestamp::now().toString(), msg);
            break;
        case LogLevel::DEBUG :
            std::println("{} [DEBUG]:{}", Timestamp::now().toString(), msg);
            break;
        case LogLevel::FATAL :
            std::println("{} [FATAL]:{}", Timestamp::now().toString(), msg);
            break;
        default :
            break;
    }
}

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

}    // namespace ltt