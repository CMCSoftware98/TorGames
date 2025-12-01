// TorGames.Installer - Simple logging
#pragma once

#include "common.h"

#define LOG_INFO(fmt, ...) Logger::Instance().Log("INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::Instance().Log("WARN", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Instance().Log("ERROR", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::Instance().Log("DEBUG", fmt, ##__VA_ARGS__)

class Logger {
public:
    static Logger& Instance();

    void Log(const char* level, const char* fmt, ...);

private:
    Logger() = default;
    CRITICAL_SECTION m_cs;
    bool m_initialized = false;
};
