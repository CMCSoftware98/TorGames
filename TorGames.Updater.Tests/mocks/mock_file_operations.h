// TorGames.Updater.Tests - Mock File Operations
#pragma once

#include <gmock/gmock.h>
#include <string>

// Interface for file operations
struct IFileOperations {
    virtual ~IFileOperations() = default;
    virtual bool FileExists(const char* path) = 0;
    virtual bool CopyFile(const char* src, const char* dst) = 0;
    virtual bool DeleteFile(const char* path) = 0;
    virtual bool CreateDirectory(const char* path) = 0;
    virtual DWORD GetFileAttributes(const char* path) = 0;
};

// Mock implementation for testing
class MockFileOperations : public IFileOperations {
public:
    MOCK_METHOD(bool, FileExists, (const char*), (override));
    MOCK_METHOD(bool, CopyFile, (const char*, const char*), (override));
    MOCK_METHOD(bool, DeleteFile, (const char*), (override));
    MOCK_METHOD(bool, CreateDirectory, (const char*), (override));
    MOCK_METHOD(DWORD, GetFileAttributes, (const char*), (override));
};
