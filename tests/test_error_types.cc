// tests/test_error_types.cc
// Unit tests for error handling types (no network required)

#include <gtest/gtest.h>
#include "quicftp_client.h"

using namespace quicftp;

// ============================================================================
// ErrorCode tests
// ============================================================================

TEST(ErrorCodeTest, AllCodesHaveStrings) {
    // Every error code should have a non-null, non-empty string representation
    ErrorCode codes[] = {
        ErrorCode::Ok, ErrorCode::ConnectionFailed, ErrorCode::AuthenticationFailed,
        ErrorCode::FileNotFound, ErrorCode::FileOpenError, ErrorCode::UploadFailed,
        ErrorCode::DownloadFailed, ErrorCode::ServerError, ErrorCode::Timeout,
        ErrorCode::SSLError, ErrorCode::NotConnected, ErrorCode::InvalidArgument,
        ErrorCode::Unknown
    };
    for (auto code : codes) {
        const char* str = error_code_str(code);
        ASSERT_NE(str, nullptr) << "error_code_str returned nullptr for code " << static_cast<int>(code);
        EXPECT_GT(strlen(str), 0u) << "error_code_str returned empty string for code " << static_cast<int>(code);
    }
}

TEST(ErrorCodeTest, OkIsOk) {
    EXPECT_STREQ(error_code_str(ErrorCode::Ok), "Ok");
}

TEST(ErrorCodeTest, SpecificCodes) {
    EXPECT_STREQ(error_code_str(ErrorCode::FileNotFound), "FileNotFound");
    EXPECT_STREQ(error_code_str(ErrorCode::ConnectionFailed), "ConnectionFailed");
    EXPECT_STREQ(error_code_str(ErrorCode::SSLError), "SSLError");
}

// ============================================================================
// TransferResult tests
// ============================================================================

TEST(TransferResultTest, OkResult) {
    auto result = TransferResult::ok(1024);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error, ErrorCode::Ok);
    EXPECT_EQ(result.bytes_transferred, 1024u);
    EXPECT_TRUE(result.message.empty());
    EXPECT_EQ(result.http_status, 0);
    EXPECT_TRUE(static_cast<bool>(result)); // bool conversion
}

TEST(TransferResultTest, OkDefaultBytes) {
    auto result = TransferResult::ok();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.bytes_transferred, 0u);
}

TEST(TransferResultTest, FailResult) {
    auto result = TransferResult::fail(ErrorCode::FileNotFound, "File not found", 404);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::FileNotFound);
    EXPECT_EQ(result.message, "File not found");
    EXPECT_EQ(result.http_status, 404);
    EXPECT_FALSE(static_cast<bool>(result)); // bool conversion
}

TEST(TransferResultTest, FailWithoutHttpStatus) {
    auto result = TransferResult::fail(ErrorCode::FileOpenError, "Cannot open file");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.http_status, 0);
}

TEST(TransferResultTest, DefaultConstructor) {
    TransferResult result;
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::Ok); // Default, but success is false
    EXPECT_EQ(result.http_status, 0);
    EXPECT_EQ(result.bytes_transferred, 0u);
}

// ============================================================================
// BatchTransferResult tests
// ============================================================================

TEST(BatchTransferResultTest, EmptyBatch) {
    BatchTransferResult batch;
    EXPECT_FALSE(batch.all_success);
    EXPECT_EQ(batch.succeeded_count, 0u);
    EXPECT_EQ(batch.failed_count, 0u);
    EXPECT_TRUE(batch.results.empty());
}

TEST(BatchTransferResultTest, AllSuccess) {
    BatchTransferResult batch;
    batch.results.push_back({"file1.txt", TransferResult::ok(100)});
    batch.results.push_back({"file2.txt", TransferResult::ok(200)});
    batch.succeeded_count = 2;
    batch.failed_count = 0;
    batch.all_success = true;

    EXPECT_TRUE(static_cast<bool>(batch));
    EXPECT_EQ(batch.results.size(), 2u);
}

TEST(BatchTransferResultTest, PartialFailure) {
    BatchTransferResult batch;
    batch.results.push_back({"file1.txt", TransferResult::ok(100)});
    batch.results.push_back({"file2.txt", TransferResult::fail(ErrorCode::FileNotFound, "Not found", 404)});
    batch.succeeded_count = 1;
    batch.failed_count = 1;
    batch.all_success = false;

    EXPECT_FALSE(static_cast<bool>(batch));
    EXPECT_TRUE(batch.results[0].second.success);
    EXPECT_FALSE(batch.results[1].second.success);
    EXPECT_EQ(batch.results[1].second.http_status, 404);
}
