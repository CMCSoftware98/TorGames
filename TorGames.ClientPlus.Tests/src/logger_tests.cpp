// TorGames.ClientPlus.Tests - Logger unit tests
#include <gtest/gtest.h>
#include "logger.h"
#include <thread>
#include <vector>

// ============================================================================
// Logger Singleton Tests
// ============================================================================

TEST(LoggerSingleton, ReturnsSameInstance) {
    Logger& instance1 = Logger::Instance();
    Logger& instance2 = Logger::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Logger Functionality Tests
// ============================================================================

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear logger before each test
        Logger::Instance().Clear();
    }

    void TearDown() override {
        // Clear logger after each test
        Logger::Instance().Clear();
    }
};

TEST_F(LoggerTest, InfoLogging) {
    LOG_INFO("Test info message");
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("INFO") != std::string::npos);
    EXPECT_TRUE(logs.find("Test info message") != std::string::npos);
}

TEST_F(LoggerTest, DebugLogging) {
    LOG_DEBUG("Test debug message");
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("DEBUG") != std::string::npos);
    EXPECT_TRUE(logs.find("Test debug message") != std::string::npos);
}

TEST_F(LoggerTest, WarningLogging) {
    LOG_WARN("Test warning message");
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("WARN") != std::string::npos);
    EXPECT_TRUE(logs.find("Test warning message") != std::string::npos);
}

TEST_F(LoggerTest, ErrorLogging) {
    LOG_ERROR("Test error message");
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("ERROR") != std::string::npos);
    EXPECT_TRUE(logs.find("Test error message") != std::string::npos);
}

TEST_F(LoggerTest, FormattedLogging) {
    LOG_INFO("Value: %d, String: %s", 42, "test");
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("Value: 42, String: test") != std::string::npos);
}

TEST_F(LoggerTest, ClearLogs) {
    LOG_INFO("Message before clear");
    Logger::Instance().Clear();
    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.empty() || logs.find("Message before clear") == std::string::npos);
}

TEST_F(LoggerTest, GetLogsLimitedCount) {
    // Log more messages than requested
    for (int i = 0; i < 10; i++) {
        LOG_INFO("Message %d", i);
    }

    std::string logs = Logger::Instance().GetLogs(3);

    // Should contain the last 3 messages
    EXPECT_TRUE(logs.find("Message 7") != std::string::npos);
    EXPECT_TRUE(logs.find("Message 8") != std::string::npos);
    EXPECT_TRUE(logs.find("Message 9") != std::string::npos);
}

TEST_F(LoggerTest, MultipleMessages) {
    LOG_INFO("First message");
    LOG_WARN("Second message");
    LOG_ERROR("Third message");

    std::string logs = Logger::Instance().GetLogs(10);
    EXPECT_TRUE(logs.find("First message") != std::string::npos);
    EXPECT_TRUE(logs.find("Second message") != std::string::npos);
    EXPECT_TRUE(logs.find("Third message") != std::string::npos);
}

// ============================================================================
// CriticalSectionGuard Tests
// ============================================================================

TEST(CriticalSectionGuardTest, AcquiresAndReleasesLock) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    {
        CriticalSectionGuard guard(&cs);
        // Lock should be held here
        // If we can create the guard without deadlock, it works
    }
    // Lock should be released here

    // Verify we can enter again (proves it was released)
    {
        CriticalSectionGuard guard2(&cs);
    }

    DeleteCriticalSection(&cs);
    SUCCEED();
}

TEST(CriticalSectionGuardTest, ReleasesOnException) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    try {
        CriticalSectionGuard guard(&cs);
        throw std::runtime_error("Test exception");
    } catch (...) {
        // Exception should have been thrown
    }

    // Verify we can enter again (proves it was released despite exception)
    {
        CriticalSectionGuard guard2(&cs);
    }

    DeleteCriticalSection(&cs);
    SUCCEED();
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(LoggerTest, ConcurrentLogging) {
    const int numThreads = 10;
    const int logsPerThread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([t, logsPerThread]() {
            for (int i = 0; i < logsPerThread; i++) {
                LOG_INFO("Thread %d, message %d", t, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // If we get here without deadlock or crash, concurrent logging works
    std::string logs = Logger::Instance().GetLogs(200);
    EXPECT_FALSE(logs.empty());
}
