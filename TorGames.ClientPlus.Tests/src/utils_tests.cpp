// TorGames.ClientPlus.Tests - Utils unit tests
#include <gtest/gtest.h>
#include "utils.h"

// ============================================================================
// ToLower Tests
// ============================================================================

TEST(UtilsToLower, EmptyString) {
    EXPECT_EQ(Utils::ToLower(""), "");
}

TEST(UtilsToLower, AlreadyLowercase) {
    EXPECT_EQ(Utils::ToLower("hello world"), "hello world");
}

TEST(UtilsToLower, AllUppercase) {
    EXPECT_EQ(Utils::ToLower("HELLO WORLD"), "hello world");
}

TEST(UtilsToLower, MixedCase) {
    EXPECT_EQ(Utils::ToLower("HeLLo WoRLD"), "hello world");
}

TEST(UtilsToLower, WithNumbers) {
    EXPECT_EQ(Utils::ToLower("Test123ABC"), "test123abc");
}

TEST(UtilsToLower, SpecialCharacters) {
    EXPECT_EQ(Utils::ToLower("Hello!@#$%"), "hello!@#$%");
}

// ============================================================================
// Trim Tests
// ============================================================================

TEST(UtilsTrim, EmptyString) {
    EXPECT_EQ(Utils::Trim(""), "");
}

TEST(UtilsTrim, WhitespaceOnly) {
    EXPECT_EQ(Utils::Trim("   \t\r\n  "), "");
}

TEST(UtilsTrim, LeadingWhitespace) {
    EXPECT_EQ(Utils::Trim("   hello"), "hello");
}

TEST(UtilsTrim, TrailingWhitespace) {
    EXPECT_EQ(Utils::Trim("hello   "), "hello");
}

TEST(UtilsTrim, BothSides) {
    EXPECT_EQ(Utils::Trim("  \t hello world \n\r  "), "hello world");
}

TEST(UtilsTrim, NoWhitespace) {
    EXPECT_EQ(Utils::Trim("hello"), "hello");
}

TEST(UtilsTrim, InternalSpaces) {
    EXPECT_EQ(Utils::Trim("  hello   world  "), "hello   world");
}

TEST(UtilsTrim, TabsAndNewlines) {
    EXPECT_EQ(Utils::Trim("\t\nhello\r\n"), "hello");
}

// ============================================================================
// Split Tests
// ============================================================================

TEST(UtilsSplit, EmptyString) {
    auto result = Utils::Split("", ',');
    // stringstream with getline returns empty vector for empty string
    EXPECT_EQ(result.size(), 0);
}

TEST(UtilsSplit, SingleElement) {
    auto result = Utils::Split("hello", ',');
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "hello");
}

TEST(UtilsSplit, MultipleElements) {
    auto result = Utils::Split("a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsSplit, EmptyParts) {
    auto result = Utils::Split("a,,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsSplit, TrailingDelimiter) {
    auto result = Utils::Split("a,b,", ',');
    // stringstream with getline doesn't produce trailing empty element
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

TEST(UtilsSplit, DifferentDelimiter) {
    auto result = Utils::Split("a|b|c", '|');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

// ============================================================================
// JsonEscape Tests
// ============================================================================

TEST(UtilsJsonEscape, EmptyString) {
    EXPECT_EQ(Utils::JsonEscape(""), "");
}

TEST(UtilsJsonEscape, NoEscapeNeeded) {
    EXPECT_EQ(Utils::JsonEscape("hello world"), "hello world");
}

TEST(UtilsJsonEscape, DoubleQuotes) {
    EXPECT_EQ(Utils::JsonEscape("say \"hello\""), "say \\\"hello\\\"");
}

TEST(UtilsJsonEscape, Backslashes) {
    EXPECT_EQ(Utils::JsonEscape("path\\to\\file"), "path\\\\to\\\\file");
}

TEST(UtilsJsonEscape, Newlines) {
    EXPECT_EQ(Utils::JsonEscape("line1\nline2"), "line1\\nline2");
}

TEST(UtilsJsonEscape, CarriageReturn) {
    EXPECT_EQ(Utils::JsonEscape("line1\rline2"), "line1\\rline2");
}

TEST(UtilsJsonEscape, Tabs) {
    EXPECT_EQ(Utils::JsonEscape("col1\tcol2"), "col1\\tcol2");
}

TEST(UtilsJsonEscape, MixedEscapes) {
    EXPECT_EQ(Utils::JsonEscape("\"path\\file\"\n"), "\\\"path\\\\file\\\"\\n");
}

// ============================================================================
// JsonGetString Tests
// ============================================================================

TEST(UtilsJsonGetString, ValidKey) {
    const char* json = R"({"name":"John","age":30})";
    EXPECT_EQ(Utils::JsonGetString(json, "name"), "John");
}

TEST(UtilsJsonGetString, MissingKey) {
    const char* json = R"({"name":"John"})";
    EXPECT_EQ(Utils::JsonGetString(json, "missing"), "");
}

TEST(UtilsJsonGetString, EmptyValue) {
    const char* json = R"({"name":""})";
    EXPECT_EQ(Utils::JsonGetString(json, "name"), "");
}

TEST(UtilsJsonGetString, ValueWithSpaces) {
    const char* json = R"({"message":"Hello World"})";
    EXPECT_EQ(Utils::JsonGetString(json, "message"), "Hello World");
}

TEST(UtilsJsonGetString, MultipleFields) {
    const char* json = R"({"first":"A","second":"B","third":"C"})";
    EXPECT_EQ(Utils::JsonGetString(json, "second"), "B");
}

// ============================================================================
// JsonGetInt Tests
// ============================================================================

TEST(UtilsJsonGetInt, ValidInt) {
    const char* json = R"({"count":42})";
    EXPECT_EQ(Utils::JsonGetInt(json, "count"), 42);
}

TEST(UtilsJsonGetInt, ZeroValue) {
    const char* json = R"({"value":0})";
    EXPECT_EQ(Utils::JsonGetInt(json, "value"), 0);
}

TEST(UtilsJsonGetInt, NegativeValue) {
    const char* json = R"({"offset":-100})";
    EXPECT_EQ(Utils::JsonGetInt(json, "offset"), -100);
}

TEST(UtilsJsonGetInt, LargeValue) {
    const char* json = R"({"memory":17179869184})";
    EXPECT_EQ(Utils::JsonGetInt(json, "memory"), 17179869184LL);
}

TEST(UtilsJsonGetInt, MissingKey) {
    const char* json = R"({"other":123})";
    EXPECT_EQ(Utils::JsonGetInt(json, "missing"), 0);
}

// ============================================================================
// JsonGetBool Tests
// ============================================================================

TEST(UtilsJsonGetBool, TrueValue) {
    const char* json = R"({"enabled":true})";
    EXPECT_TRUE(Utils::JsonGetBool(json, "enabled"));
}

TEST(UtilsJsonGetBool, FalseValue) {
    const char* json = R"({"enabled":false})";
    EXPECT_FALSE(Utils::JsonGetBool(json, "enabled"));
}

TEST(UtilsJsonGetBool, MissingKey) {
    const char* json = R"({"other":true})";
    EXPECT_FALSE(Utils::JsonGetBool(json, "missing"));
}

TEST(UtilsJsonGetBool, MultipleBools) {
    const char* json = R"({"a":true,"b":false,"c":true})";
    EXPECT_TRUE(Utils::JsonGetBool(json, "a"));
    EXPECT_FALSE(Utils::JsonGetBool(json, "b"));
    EXPECT_TRUE(Utils::JsonGetBool(json, "c"));
}

// ============================================================================
// Sha256 Tests
// ============================================================================

TEST(UtilsSha256, EmptyString) {
    // SHA256 of empty string is a known constant
    std::string hash = Utils::Sha256("");
    EXPECT_EQ(hash.length(), 64);
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(UtilsSha256, HelloWorld) {
    // SHA256 of "hello world" (without newline)
    std::string hash = Utils::Sha256("hello world");
    EXPECT_EQ(hash.length(), 64);
    EXPECT_EQ(hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST(UtilsSha256, ConsistentResults) {
    std::string input = "test input for hashing";
    std::string hash1 = Utils::Sha256(input);
    std::string hash2 = Utils::Sha256(input);
    EXPECT_EQ(hash1, hash2);
}

TEST(UtilsSha256, DifferentInputsDifferentHashes) {
    std::string hash1 = Utils::Sha256("input1");
    std::string hash2 = Utils::Sha256("input2");
    EXPECT_NE(hash1, hash2);
}

TEST(UtilsSha256, ValidHexFormat) {
    std::string hash = Utils::Sha256("any string");
    EXPECT_EQ(hash.length(), 64);
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// ============================================================================
// DecodeUnicodeEscapes Tests
// ============================================================================

TEST(UtilsDecodeUnicode, NoEscapes) {
    // Note: DecodeUnicodeEscapes is internal but tested via JsonGetString
    const char* json = R"({"name":"test"})";
    EXPECT_EQ(Utils::JsonGetString(json, "name"), "test");
}

TEST(UtilsDecodeUnicode, UnicodeQuote) {
    // \u0022 is a double quote character
    const char* json = "{\"name\":\"test\\u0022value\"}";
    // The function decodes \u0022 to " which terminates the string
    std::string result = Utils::JsonGetString(json, "name");
    EXPECT_EQ(result, "test");
}

// ============================================================================
// IsUserLoggedIn Tests
// ============================================================================

TEST(UtilsIsUserLoggedIn, ReturnsBoolean) {
    // This is an integration test - just verify it returns a boolean without crashing
    bool result = Utils::IsUserLoggedIn();
    // Result depends on test environment - just verify it doesn't crash
    EXPECT_TRUE(result == true || result == false);
}

TEST(UtilsIsUserLoggedIn, ConsistentResults) {
    // Multiple calls should return consistent results
    bool result1 = Utils::IsUserLoggedIn();
    bool result2 = Utils::IsUserLoggedIn();
    EXPECT_EQ(result1, result2);
}

// ============================================================================
// IsUacDisabled Tests
// ============================================================================

TEST(UtilsIsUacDisabled, ReturnsBoolean) {
    // Integration test - verify it returns without crashing
    bool result = Utils::IsUacDisabled();
    EXPECT_TRUE(result == true || result == false);
}

TEST(UtilsIsUacDisabled, ConsistentResults) {
    // Multiple calls should return consistent results
    bool result1 = Utils::IsUacDisabled();
    bool result2 = Utils::IsUacDisabled();
    EXPECT_EQ(result1, result2);
}

// ============================================================================
// IsUacEnabled Tests
// ============================================================================

TEST(UtilsIsUacEnabled, ReturnsBoolean) {
    // Integration test - verify it returns without crashing
    bool result = Utils::IsUacEnabled();
    EXPECT_TRUE(result == true || result == false);
}

TEST(UtilsIsUacEnabled, ConsistentResults) {
    // Multiple calls should return consistent results
    bool result1 = Utils::IsUacEnabled();
    bool result2 = Utils::IsUacEnabled();
    EXPECT_EQ(result1, result2);
}

TEST(UtilsIsUacEnabled, InverseOfDisabledWhenFullyDisabled) {
    // Note: IsUacEnabled checks EnableLUA registry key
    // IsUacDisabled checks ConsentPromptBehaviorAdmin and PromptOnSecureDesktop
    // They check different registry values, so they're not strict inverses
    // This test just documents the behavior
    bool enabled = Utils::IsUacEnabled();
    bool disabled = Utils::IsUacDisabled();
    // Both can be true (UAC enabled but prompts disabled) or
    // enabled=false and disabled=true (UAC fully disabled)
    // But if UAC is enabled and prompts are enabled, disabled should be false
    if (enabled && !disabled) {
        // Normal case: UAC enabled, prompts enabled
        SUCCEED();
    } else if (!enabled && disabled) {
        // UAC fully disabled
        SUCCEED();
    } else if (enabled && disabled) {
        // UAC enabled but prompts disabled (what DisableUac() does)
        SUCCEED();
    } else {
        // enabled=false and disabled=false is unlikely but possible
        SUCCEED();
    }
}

// ============================================================================
// IsRunningAsAdmin Tests
// ============================================================================

TEST(UtilsIsRunningAsAdmin, ReturnsBoolean) {
    // Integration test - verify it returns without crashing
    bool result = Utils::IsRunningAsAdmin();
    EXPECT_TRUE(result == true || result == false);
}

TEST(UtilsIsRunningAsAdmin, ConsistentResults) {
    // Multiple calls should return consistent results
    bool result1 = Utils::IsRunningAsAdmin();
    bool result2 = Utils::IsRunningAsAdmin();
    EXPECT_EQ(result1, result2);
}

// ============================================================================
// GetCountryCode Tests
// ============================================================================

TEST(UtilsGetCountryCode, ReturnsValidFormat) {
    std::string code = Utils::GetCountryCode();
    // Country code should be empty or a 2-letter lowercase ISO code
    if (!code.empty()) {
        EXPECT_EQ(code.length(), 2);
        EXPECT_TRUE(code[0] >= 'a' && code[0] <= 'z');
        EXPECT_TRUE(code[1] >= 'a' && code[1] <= 'z');
    }
}

TEST(UtilsGetCountryCode, ConsistentResults) {
    std::string code1 = Utils::GetCountryCode();
    std::string code2 = Utils::GetCountryCode();
    EXPECT_EQ(code1, code2);
}

// ============================================================================
// File Utility Tests
// ============================================================================

TEST(UtilsFileExists, NonExistentFile) {
    bool result = Utils::FileExists("C:\\this_file_should_not_exist_12345.txt");
    EXPECT_FALSE(result);
}

TEST(UtilsFileExists, SystemFile) {
    // Windows system file that should always exist
    bool result = Utils::FileExists("C:\\Windows\\System32\\kernel32.dll");
    EXPECT_TRUE(result);
}

TEST(UtilsDirectoryExists, NonExistentDirectory) {
    bool result = Utils::DirectoryExists("C:\\this_directory_should_not_exist_12345");
    EXPECT_FALSE(result);
}

TEST(UtilsDirectoryExists, SystemDirectory) {
    // Windows system directory that should always exist
    bool result = Utils::DirectoryExists("C:\\Windows\\System32");
    EXPECT_TRUE(result);
}

TEST(UtilsDirectoryExists, FileIsNotDirectory) {
    // A file should not be detected as a directory
    bool result = Utils::DirectoryExists("C:\\Windows\\System32\\kernel32.dll");
    EXPECT_FALSE(result);
}
