// TorGames.Installer - Simple logging implementation
#include "logger.h"
#include <stdarg.h>

Logger& Logger::Instance() {
    static Logger instance;
    if (!instance.m_initialized) {
        InitializeCriticalSection(&instance.m_cs);
        instance.m_initialized = true;
    }
    return instance;
}

void Logger::Log(const char* level, const char* fmt, ...) {
    EnterCriticalSection(&m_cs);

    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d]",
        st.wHour, st.wMinute, st.wSecond);

    // Format message
    char message[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // Print to console
    printf("%s [%s] %s\n", timestamp, level, message);

    LeaveCriticalSection(&m_cs);
}
