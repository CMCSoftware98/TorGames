// TorGames.ClientPlus - Logger implementation
#include "logger.h"
#include <stdarg.h>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_head(0), m_count(0) {
    InitializeCriticalSection(&m_cs);
    memset(m_entries, 0, sizeof(m_entries));
    m_logPath[0] = '\0';

    // Default log path
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    snprintf(m_logPath, sizeof(m_logPath), "%sTorGames_ClientPlus.log", temp);
}

Logger::~Logger() {
    DeleteCriticalSection(&m_cs);
}

void Logger::SetLogFile(const char* path) {
    EnterCriticalSection(&m_cs);
    strncpy(m_logPath, path, MAX_PATH - 1);
    LeaveCriticalSection(&m_cs);
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddEntry(level, buf);
}

void Logger::Debug(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddEntry(LogLevel::Debug, buf);
}

void Logger::Info(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddEntry(LogLevel::Info, buf);
}

void Logger::Warning(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddEntry(LogLevel::Warning, buf);
}

void Logger::Error(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddEntry(LogLevel::Error, buf);
}

void Logger::AddEntry(LogLevel level, const char* message) {
    EnterCriticalSection(&m_cs);

    LogEntry& entry = m_entries[m_head];
    GetLocalTime(&entry.timestamp);
    entry.level = level;
    strncpy(entry.message, message, sizeof(entry.message) - 1);

    // Print to console
    printf("[%02d:%02d:%02d] [%s] %s\n",
        entry.timestamp.wHour, entry.timestamp.wMinute, entry.timestamp.wSecond,
        LevelToString(level), message);

    // Write to log file
    if (m_logPath[0]) {
        FILE* f = fopen(m_logPath, "a");
        if (f) {
            fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
                entry.timestamp.wYear, entry.timestamp.wMonth, entry.timestamp.wDay,
                entry.timestamp.wHour, entry.timestamp.wMinute, entry.timestamp.wSecond,
                LevelToString(level), message);
            fclose(f);
        }
    }

    m_head = (m_head + 1) % MAX_LOG_ENTRIES;
    if (m_count < MAX_LOG_ENTRIES) m_count++;

    LeaveCriticalSection(&m_cs);
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetLogs(int maxLines) {
    EnterCriticalSection(&m_cs);

    std::string result;
    int linesToGet = (maxLines > m_count) ? m_count : maxLines;
    int start = (m_head - linesToGet + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;

    for (int i = 0; i < linesToGet; i++) {
        int idx = (start + i) % MAX_LOG_ENTRIES;
        const LogEntry& entry = m_entries[idx];

        char line[600];
        snprintf(line, sizeof(line), "[%02d:%02d:%02d] [%s] %s\n",
            entry.timestamp.wHour, entry.timestamp.wMinute, entry.timestamp.wSecond,
            LevelToString(entry.level), entry.message);
        result += line;
    }

    LeaveCriticalSection(&m_cs);
    return result;
}

void Logger::Clear() {
    EnterCriticalSection(&m_cs);
    m_head = 0;
    m_count = 0;
    memset(m_entries, 0, sizeof(m_entries));
    LeaveCriticalSection(&m_cs);
}
