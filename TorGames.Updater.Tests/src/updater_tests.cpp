// TorGames.Updater.Tests - Updater unit tests
#include <gtest/gtest.h>
#include "updater_functions.h"

using namespace UpdaterFunctions;

// ============================================================================
// GetDirectoryFromPath Tests
// ============================================================================

TEST(UpdaterGetDirectory, ValidPath) {
    EXPECT_EQ(GetDirectoryFromPath("C:\\Users\\Test\\file.exe"), "C:\\Users\\Test");
}

TEST(UpdaterGetDirectory, RootPath) {
    EXPECT_EQ(GetDirectoryFromPath("C:\\file.exe"), "C:");
}

TEST(UpdaterGetDirectory, NoBackslash) {
    EXPECT_EQ(GetDirectoryFromPath("file.exe"), "");
}

TEST(UpdaterGetDirectory, DeepPath) {
    EXPECT_EQ(GetDirectoryFromPath("C:\\A\\B\\C\\D\\file.exe"), "C:\\A\\B\\C\\D");
}

// ============================================================================
// BuildCommandLine Tests
// ============================================================================

TEST(UpdaterBuildCommandLine, WithArguments) {
    std::string cmd = BuildCommandLine("C:\\test\\app.exe", "--flag value");
    EXPECT_EQ(cmd, "\"C:\\test\\app.exe\" --flag value");
}

TEST(UpdaterBuildCommandLine, NoArguments) {
    std::string cmd = BuildCommandLine("C:\\test\\app.exe", nullptr);
    EXPECT_EQ(cmd, "\"C:\\test\\app.exe\"");
}

TEST(UpdaterBuildCommandLine, EmptyArguments) {
    std::string cmd = BuildCommandLine("C:\\test\\app.exe", "");
    EXPECT_EQ(cmd, "\"C:\\test\\app.exe\"");
}

TEST(UpdaterBuildCommandLine, PathWithSpaces) {
    std::string cmd = BuildCommandLine("C:\\Program Files\\app.exe", "--post-update");
    EXPECT_EQ(cmd, "\"C:\\Program Files\\app.exe\" --post-update");
}

// ============================================================================
// GenerateBackupFileName Tests
// ============================================================================

TEST(UpdaterBackupFileName, StandardTime) {
    SYSTEMTIME st = {};
    st.wYear = 2025;
    st.wMonth = 12;
    st.wDay = 25;
    st.wHour = 14;
    st.wMinute = 30;
    st.wSecond = 45;

    std::string filename = GenerateBackupFileName(st);
    EXPECT_EQ(filename, "TorGames.Client_20251225_143045.exe.bak");
}

TEST(UpdaterBackupFileName, SingleDigitValues) {
    SYSTEMTIME st = {};
    st.wYear = 2025;
    st.wMonth = 1;
    st.wDay = 5;
    st.wHour = 9;
    st.wMinute = 8;
    st.wSecond = 7;

    std::string filename = GenerateBackupFileName(st);
    EXPECT_EQ(filename, "TorGames.Client_20250105_090807.exe.bak");
}

TEST(UpdaterBackupFileName, Midnight) {
    SYSTEMTIME st = {};
    st.wYear = 2025;
    st.wMonth = 6;
    st.wDay = 15;
    st.wHour = 0;
    st.wMinute = 0;
    st.wSecond = 0;

    std::string filename = GenerateBackupFileName(st);
    EXPECT_EQ(filename, "TorGames.Client_20250615_000000.exe.bak");
}

// ============================================================================
// BuildBackupPath Tests
// ============================================================================

TEST(UpdaterBuildBackupPath, StandardPath) {
    std::string path = BuildBackupPath("C:\\Backups", "backup_20251225.bak");
    EXPECT_EQ(path, "C:\\Backups\\backup_20251225.bak");
}

TEST(UpdaterBuildBackupPath, TrailingBackslash) {
    // Note: The function doesn't strip trailing backslash, so it will double
    std::string path = BuildBackupPath("C:\\Backups\\", "backup.bak");
    EXPECT_EQ(path, "C:\\Backups\\\\backup.bak");
}

// ============================================================================
// ParseArguments Tests
// ============================================================================

TEST(UpdaterParseArguments, ValidMinimalArgs) {
    char* argv[] = {
        (char*)"updater.exe",
        (char*)"C:\\target.exe",
        (char*)"C:\\new.exe",
        (char*)"C:\\backups",
        (char*)"1234"
    };

    UpdateArgs args = ParseArguments(5, argv);

    EXPECT_TRUE(args.isValid);
    EXPECT_EQ(args.targetPath, "C:\\target.exe");
    EXPECT_EQ(args.newFilePath, "C:\\new.exe");
    EXPECT_EQ(args.backupDir, "C:\\backups");
    EXPECT_EQ(args.parentPid, 1234);
    EXPECT_EQ(args.version, "");
}

TEST(UpdaterParseArguments, WithVersion) {
    char* argv[] = {
        (char*)"updater.exe",
        (char*)"C:\\target.exe",
        (char*)"C:\\new.exe",
        (char*)"C:\\backups",
        (char*)"5678",
        (char*)"1.2.3"
    };

    UpdateArgs args = ParseArguments(6, argv);

    EXPECT_TRUE(args.isValid);
    EXPECT_EQ(args.parentPid, 5678);
    EXPECT_EQ(args.version, "1.2.3");
}

TEST(UpdaterParseArguments, InsufficientArgs) {
    char* argv[] = {
        (char*)"updater.exe",
        (char*)"C:\\target.exe",
        (char*)"C:\\new.exe"
    };

    UpdateArgs args = ParseArguments(3, argv);

    EXPECT_FALSE(args.isValid);
}

TEST(UpdaterParseArguments, OnlyProgramName) {
    char* argv[] = {
        (char*)"updater.exe"
    };

    UpdateArgs args = ParseArguments(1, argv);

    EXPECT_FALSE(args.isValid);
}

// ============================================================================
// SortBackupFilesDescending Tests
// ============================================================================

TEST(UpdaterSortBackups, AlreadySorted) {
    std::string files[3] = {
        "TorGames.Client_20251203_120000.exe.bak",
        "TorGames.Client_20251202_120000.exe.bak",
        "TorGames.Client_20251201_120000.exe.bak"
    };

    SortBackupFilesDescending(files, 3);

    EXPECT_EQ(files[0], "TorGames.Client_20251203_120000.exe.bak");
    EXPECT_EQ(files[1], "TorGames.Client_20251202_120000.exe.bak");
    EXPECT_EQ(files[2], "TorGames.Client_20251201_120000.exe.bak");
}

TEST(UpdaterSortBackups, ReverseSorted) {
    std::string files[3] = {
        "TorGames.Client_20251201_120000.exe.bak",
        "TorGames.Client_20251202_120000.exe.bak",
        "TorGames.Client_20251203_120000.exe.bak"
    };

    SortBackupFilesDescending(files, 3);

    EXPECT_EQ(files[0], "TorGames.Client_20251203_120000.exe.bak");
    EXPECT_EQ(files[1], "TorGames.Client_20251202_120000.exe.bak");
    EXPECT_EQ(files[2], "TorGames.Client_20251201_120000.exe.bak");
}

TEST(UpdaterSortBackups, MixedOrder) {
    std::string files[4] = {
        "TorGames.Client_20251202_120000.exe.bak",
        "TorGames.Client_20251204_120000.exe.bak",
        "TorGames.Client_20251201_120000.exe.bak",
        "TorGames.Client_20251203_120000.exe.bak"
    };

    SortBackupFilesDescending(files, 4);

    EXPECT_EQ(files[0], "TorGames.Client_20251204_120000.exe.bak");
    EXPECT_EQ(files[1], "TorGames.Client_20251203_120000.exe.bak");
    EXPECT_EQ(files[2], "TorGames.Client_20251202_120000.exe.bak");
    EXPECT_EQ(files[3], "TorGames.Client_20251201_120000.exe.bak");
}

TEST(UpdaterSortBackups, SingleElement) {
    std::string files[1] = {
        "TorGames.Client_20251201_120000.exe.bak"
    };

    SortBackupFilesDescending(files, 1);

    EXPECT_EQ(files[0], "TorGames.Client_20251201_120000.exe.bak");
}

TEST(UpdaterSortBackups, SameDay) {
    std::string files[3] = {
        "TorGames.Client_20251201_100000.exe.bak",
        "TorGames.Client_20251201_140000.exe.bak",
        "TorGames.Client_20251201_120000.exe.bak"
    };

    SortBackupFilesDescending(files, 3);

    EXPECT_EQ(files[0], "TorGames.Client_20251201_140000.exe.bak");
    EXPECT_EQ(files[1], "TorGames.Client_20251201_120000.exe.bak");
    EXPECT_EQ(files[2], "TorGames.Client_20251201_100000.exe.bak");
}

// ============================================================================
// CreateDirectoryRecursive Integration Tests
// ============================================================================

TEST(UpdaterCreateDir, TempDirectory) {
    // Get temp path
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    // Create a unique test path
    char testPath[MAX_PATH];
    snprintf(testPath, sizeof(testPath), "%sUpdaterTest_%lu\\nested\\dir",
        tempPath, GetTickCount());

    // Create the directory
    bool result = CreateDirectoryRecursive(testPath);
    EXPECT_TRUE(result);

    // Verify it exists
    DWORD attr = GetFileAttributesA(testPath);
    EXPECT_NE(attr, INVALID_FILE_ATTRIBUTES);
    EXPECT_TRUE((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);

    // Cleanup
    RemoveDirectoryA(testPath);
    char parent1[MAX_PATH];
    snprintf(parent1, sizeof(parent1), "%sUpdaterTest_%lu\\nested",
        tempPath, GetTickCount());
    RemoveDirectoryA(parent1);
    char parent2[MAX_PATH];
    snprintf(parent2, sizeof(parent2), "%sUpdaterTest_%lu",
        tempPath, GetTickCount());
    RemoveDirectoryA(parent2);
}

TEST(UpdaterCreateDir, ExistingDirectory) {
    // Get temp path (this should always exist)
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    // Remove trailing backslash for the test
    size_t len = strlen(tempPath);
    if (len > 0 && tempPath[len - 1] == '\\') {
        tempPath[len - 1] = '\0';
    }

    // CreateDirectoryRecursive should return true for existing directories
    bool result = CreateDirectoryRecursive(tempPath);
    EXPECT_TRUE(result);
}
