// TorGames.ClientPlus - Logger
#pragma once

#include "common.h"

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry {
    SYSTEMTIME timestamp;
    LogLevel level;
    char message[512];
};

class Logger {
public:
    static Logger& Instance();

    void Log(LogLevel level, const char* fmt, ...);
    void Debug(const char* fmt, ...);
    void Info(const char* fmt, ...);
    void Warning(const char* fmt, ...);
    void Error(const char* fmt, ...);

    std::string GetLogs(int maxLines = 200);
    void Clear();

    void SetLogFile(const char* path);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void AddEntry(LogLevel level, const char* message);
    const char* LevelToString(LogLevel level);

    LogEntry m_entries[MAX_LOG_ENTRIES];
    int m_head;
    int m_count;
    CRITICAL_SECTION m_cs;
    char m_logPath[MAX_PATH];
};

// Convenience macro
#define LOG_INFO(...) Logger::Instance().Info(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Instance().Debug(__VA_ARGS__)
#define LOG_WARN(...) Logger::Instance().Warning(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Instance().Error(__VA_ARGS__)
