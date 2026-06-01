module Muduo.Logger;

import Muduo.Timestamp;

namespace ltt
{

void Logger::log(LogLevel level, std::string_view msg)
{
    constexpr std::string_view white_red = "\033[37;41m";    // FATAL (白字红底)
    constexpr std::string_view red       = "\033[31m";       // ERROR
    constexpr std::string_view green     = "\033[32m";       // INFO
    constexpr std::string_view blue      = "\033[34m";       // DEBUG
    constexpr std::string_view cyan      = "\033[36m";       // FUNC_BEGIN
    constexpr std::string_view magenta   = "\033[35m";       // FUNC_END
    constexpr std::string_view reset     = "\033[0m";        // 重置颜色

    switch (level)
    {
        case LogLevel::FATAL :
            std::println("[{}] {}[FATAL]{}{}", Timestamp::now().to_string(), white_red, reset, msg);
            break;
        case LogLevel::ERROR : std::println("[{}] {}[ERROR]{}{}", Timestamp::now().to_string(), red, reset, msg); break;
        case LogLevel::INFO :
            std::println("[{}] {}[INFO]{} {}", Timestamp::now().to_string(), green, reset, msg);
            break;
        case LogLevel::DEBUG :
            std::println("[{}] {}[DEBUG]{}{}", Timestamp::now().to_string(), blue, reset, msg);
            break;
        case LogLevel::FUNC_BEGIN :
            std::println("[{}] {}[FUNC_INFO]{} {}{}{}", Timestamp::now().to_string(), blue, reset, cyan, msg, reset);
            break;
        case LogLevel::FUNC_END :
            std::println("[{}] {}[FUNC_INFO]{} {}{}{}", Timestamp::now().to_string(), blue, reset, magenta, msg, reset);
            break;
        default : break;
    }
}

}    // namespace ltt
