// TorGames.ClientPlus.Tests - Mock File System
#pragma once

#include <gmock/gmock.h>
#include <string>
#include <vector>

// Interface for file system operations
struct IFileSystem {
    virtual ~IFileSystem() = default;
    virtual bool FileExists(const char* path) = 0;
    virtual bool DirectoryExists(const char* path) = 0;
    virtual bool CreateDirectoryRecursive(const char* path) = 0;
    virtual bool DeleteFile(const char* path) = 0;
    virtual bool CopyFile(const char* src, const char* dst) = 0;
};

// Mock implementation for testing
class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(bool, FileExists, (const char*), (override));
    MOCK_METHOD(bool, DirectoryExists, (const char*), (override));
    MOCK_METHOD(bool, CreateDirectoryRecursive, (const char*), (override));
    MOCK_METHOD(bool, DeleteFile, (const char*), (override));
    MOCK_METHOD(bool, CopyFile, (const char*, const char*), (override));
};
