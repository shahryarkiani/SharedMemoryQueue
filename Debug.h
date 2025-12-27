#ifndef DEBUG_H_
#define DEBUG_H_

#include <iostream>

enum class LogLevel : int {
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    ALL = 4,
};

const LogLevel LEVEL = LogLevel::ALL;

inline bool shouldLog(LogLevel msgLevel) {
    return static_cast<int>(msgLevel) <= static_cast<int>(LEVEL);
}

void _log() {
    std::cout << std::endl;
}

template<typename T, typename... Args>
void _log(T head, Args... tail) {
    std::cout << head << " ";
    _log(tail...);
}


template<typename... Args>
void LogInfo(const Args&... args) {
    if(!shouldLog(LogLevel::INFO)) return;
    std::cout << "[INFO] ";
    _log(args...);
}

template<typename... Args>
void LogWarn(const Args&... args) {
    if(!shouldLog(LogLevel::WARN)) return;
    std::cout << "[WARN] ";
    _log(args...);
}

template<typename... Args>
void LogError(const Args&... args) {
    if(!shouldLog(LogLevel::ERROR)) return;
    std::cout << "[ERROR] ";
    _log(args...);
}

#endif