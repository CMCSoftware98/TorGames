// TorGames.Installer.Tests - Utils unit tests
// Note: These tests are similar to ClientPlus utils tests as the code is nearly identical
#include <gtest/gtest.h>
#include "utils.h"

// ============================================================================
// ToLower Tests
// ============================================================================

TEST(InstallerUtilsToLower, EmptyString) {
    EXPECT_EQ(Utils::ToLower(""), "");
}

TEST(InstallerUtilsToLower, AlreadyLowercase) {
    EXPECT_EQ(Utils::ToLower("hello world"), "hello world");
}

TEST(InstallerUtilsToLower, AllUppercase) {
    EXPECT_EQ(Utils::ToLower("HELLO WORLD"), "hello world");
}

TEST(InstallerUtilsToLower, MixedCase) {
    EXPECT_EQ(Utils::ToLower("HeLLo WoRLD"), "hello world");
}

// ============================================================================
// Trim Tests
// ============================================================================

TEST(InstallerUtilsTrim, EmptyString) {
    EXPECT_EQ(Utils::Trim(""), "");
}

TEST(InstallerUtilsTrim, WhitespaceOnly) {
    EXPECT_EQ(Utils::Trim("   \t\r\n  "), "");
}

TEST(InstallerUtilsTrim, BothSides) {
    EXPECT_EQ(Utils::Trim("  \t hello world \n\r  "), "hello world");
}

// ============================================================================
// Split Tests
// ============================================================================

TEST(InstallerUtilsSplit, MultipleElements) {
    auto result = Utils::Split("a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(InstallerUtilsSplit, EmptyParts) {
    auto result = Utils::Split("a,,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[1], "");
}

// ============================================================================
// JsonEscape Tests
// ============================================================================

TEST(InstallerUtilsJsonEscape, EmptyString) {
    EXPECT_EQ(Utils::JsonEscape(""), "");
}

TEST(InstallerUtilsJsonEscape, DoubleQuotes) {
    EXPECT_EQ(Utils::JsonEscape("say \"hello\""), "say \\\"hello\\\"");
}

TEST(InstallerUtilsJsonEscape, Backslashes) {
    EXPECT_EQ(Utils::JsonEscape("path\\to\\file"), "path\\\\to\\\\file");
}

TEST(InstallerUtilsJsonEscape, MixedEscapes) {
    EXPECT_EQ(Utils::JsonEscape("\"path\\file\"\n"), "\\\"path\\\\file\\\"\\n");
}

// ============================================================================
// JsonGetString Tests
// ============================================================================

TEST(InstallerUtilsJsonGetString, ValidKey) {
    const char* json = R"({"name":"John","age":30})";
    EXPECT_EQ(Utils::JsonGetString(json, "name"), "John");
}

TEST(InstallerUtilsJsonGetString, MissingKey) {
    const char* json = R"({"name":"John"})";
    EXPECT_EQ(Utils::JsonGetString(json, "missing"), "");
}

TEST(InstallerUtilsJsonGetString, MultipleFields) {
    const char* json = R"({"first":"A","second":"B","third":"C"})";
    EXPECT_EQ(Utils::JsonGetString(json, "second"), "B");
}

// ============================================================================
// JsonGetInt Tests
// ============================================================================

TEST(InstallerUtilsJsonGetInt, ValidInt) {
    const char* json = R"({"count":42})";
    EXPECT_EQ(Utils::JsonGetInt(json, "count"), 42);
}

TEST(InstallerUtilsJsonGetInt, NegativeValue) {
    const char* json = R"({"offset":-100})";
    EXPECT_EQ(Utils::JsonGetInt(json, "offset"), -100);
}

TEST(InstallerUtilsJsonGetInt, MissingKey) {
    const char* json = R"({"other":123})";
    EXPECT_EQ(Utils::JsonGetInt(json, "missing"), 0);
}

// ============================================================================
// JsonGetBool Tests
// ============================================================================

TEST(InstallerUtilsJsonGetBool, TrueValue) {
    const char* json = R"({"enabled":true})";
    EXPECT_TRUE(Utils::JsonGetBool(json, "enabled"));
}

TEST(InstallerUtilsJsonGetBool, FalseValue) {
    const char* json = R"({"enabled":false})";
    EXPECT_FALSE(Utils::JsonGetBool(json, "enabled"));
}

TEST(InstallerUtilsJsonGetBool, MissingKey) {
    const char* json = R"({"other":true})";
    EXPECT_FALSE(Utils::JsonGetBool(json, "missing"));
}

// ============================================================================
// Sha256 Tests
// ============================================================================

TEST(InstallerUtilsSha256, EmptyString) {
    std::string hash = Utils::Sha256("");
    EXPECT_EQ(hash.length(), 64);
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(InstallerUtilsSha256, HelloWorld) {
    std::string hash = Utils::Sha256("hello world");
    EXPECT_EQ(hash.length(), 64);
    EXPECT_EQ(hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST(InstallerUtilsSha256, ConsistentResults) {
    std::string input = "test input for hashing";
    std::string hash1 = Utils::Sha256(input);
    std::string hash2 = Utils::Sha256(input);
    EXPECT_EQ(hash1, hash2);
}

// ============================================================================
// System Info Tests (Integration - uses actual system)
// ============================================================================

TEST(InstallerUtilsSystemInfo, GetMachineNameNotEmpty) {
    std::string name = Utils::GetMachineName();
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, "UNKNOWN");
}

TEST(InstallerUtilsSystemInfo, GetUsernameNotEmpty) {
    std::string username = Utils::GetUsername();
    EXPECT_FALSE(username.empty());
    EXPECT_NE(username, "UNKNOWN");
}

TEST(InstallerUtilsSystemInfo, GetOsVersionStartsWithWindows) {
    std::string osVersion = Utils::GetOsVersion();
    EXPECT_TRUE(osVersion.find("Windows") == 0);
}

TEST(InstallerUtilsSystemInfo, GetArchitectureValid) {
    std::string arch = Utils::GetArchitecture();
    EXPECT_TRUE(arch == "x64" || arch == "ARM64" || arch == "x86");
}

TEST(InstallerUtilsSystemInfo, GetCpuCountPositive) {
    int cpuCount = Utils::GetCpuCount();
    EXPECT_GT(cpuCount, 0);
}

TEST(InstallerUtilsSystemInfo, GetTotalMemoryPositive) {
    long long totalMem = Utils::GetTotalMemory();
    EXPECT_GT(totalMem, 0);
}

TEST(InstallerUtilsSystemInfo, GetAvailableMemoryPositive) {
    long long availMem = Utils::GetAvailableMemory();
    EXPECT_GT(availMem, 0);
}

TEST(InstallerUtilsSystemInfo, GetLocalIpValid) {
    std::string ip = Utils::GetLocalIp();
    // Should either be a valid IP or 0.0.0.0
    EXPECT_FALSE(ip.empty());
}

TEST(InstallerUtilsSystemInfo, GetMacAddressFormat) {
    std::string mac = Utils::GetMacAddress();
    // Should be either a valid MAC format or the default
    EXPECT_FALSE(mac.empty());
    // MAC should be 17 characters: XX:XX:XX:XX:XX:XX
    EXPECT_EQ(mac.length(), 17);
}

TEST(InstallerUtilsSystemInfo, GetHardwareIdConsistent) {
    std::string hwid1 = Utils::GetHardwareId();
    std::string hwid2 = Utils::GetHardwareId();
    EXPECT_EQ(hwid1, hwid2);
    EXPECT_EQ(hwid1.length(), 64); // SHA256 produces 64 hex chars
}
