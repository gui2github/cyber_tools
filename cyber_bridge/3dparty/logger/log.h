// cyber_logger.hpp
#ifndef CYBER_LOGGER_HPP_
#define CYBER_LOGGER_HPP_

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <ctime>

// 日志等级
enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    FATAL = 4
};

// 全局日志级别（默认 INFO）
inline LogLevel g_log_level = LogLevel::INFO;

// 前向声明日志类
class Logger;

// 便捷宏（推荐使用这种写法）
#define LOG_DEBUG Logger(LogLevel::DEBUG, __FILE__, __LINE__, __func__)
#define LOG_INFO  Logger(LogLevel::INFO,  __FILE__, __LINE__, __func__)
#define LOG_WARN  Logger(LogLevel::WARN,  __FILE__, __LINE__, __func__)
#define LOG_ERROR Logger(LogLevel::ERROR, __FILE__, __LINE__, __func__)
#define LOG_FATAL Logger(LogLevel::FATAL, __FILE__, __LINE__, __func__)

// 更接近 Apollo 风格的写法（可选）
// #define LOG(level) Logger(level, __FILE__, __LINE__, __func__)
// #define AINFO  LOG(LogLevel::INFO)
// #define AWARN  LOG(LogLevel::WARN)
// #define AERROR LOG(LogLevel::ERROR)
// #define AFATAL LOG(LogLevel::FATAL)

/**
 * 简单线程安全的流式日志类
 * 用法：
 *   LOG_WARN << "client: " << topic << " not found";
 */
class Logger {
public:
    Logger(LogLevel level, const char* file, int line, const char* func)
        : level_(level),
          file_(file),
          line_(line),
          func_(func) {

        if (level_ < g_log_level) {
            disabled_ = true;
            return;
        }

        // 前缀：时间 [LEVEL] 文件:行 (函数) 
        ss_ << GetTimeString() << " ["
            << LevelToString(level_) << "] "
            << ShortFileName(file_) << ":" << line_ << " "
            << func_ << " - ";
    }

    ~Logger() {
        if (disabled_) return;

        std::lock_guard<std::mutex> lock(GetMutex());
        std::cerr << ss_.str() << std::endl;
    }

    // 支持任意类型的流式输出
    template<typename T>
    Logger& operator<<(const T& value) {
        if (!disabled_) {
            ss_ << value;
        }
        return *this;
    }

    // 特殊处理 std::endl
    Logger& operator<<(std::ostream& (*manip)(std::ostream&)) {
        if (!disabled_) {
            manip(ss_);
        }
        return *this;
    }

private:
    std::ostringstream ss_;
    LogLevel level_;
    const char* file_;
    int line_;
    const char* func_;
    bool disabled_ = false;

    // 静态互斥锁（单例式）
    static std::mutex& GetMutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::string GetTimeString() {
        std::time_t now = std::time(nullptr);
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return buf;
    }

    static const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default:              return "?????";
        }
    }

    // 缩短文件名，只显示最后一部分（更简洁）
    static const char* ShortFileName(const char* path) {
        const char* last = path;
        for (const char* p = path; *p; ++p) {
            if (*p == '/' || *p == '\\') {
                last = p + 1;
            }
        }
        return last;
    }
};

// 可选：运行时修改日志级别
inline void SetLogLevel(LogLevel level) {
    g_log_level = level;
}

#endif  // CYBER_LOGGER_HPP_