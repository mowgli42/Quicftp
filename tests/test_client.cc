// tests/test_client.cc
// Unit tests for quicftp::Client class

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "quicftp_client.h"

using namespace quicftp;

// ============================================================================
// Client construction and configuration tests (no server needed)
// ============================================================================

class ClientTest : public ::testing::Test {
protected:
    Client client;
};

TEST_F(ClientTest, DefaultConstruction) {
    // Client should construct without errors
    SUCCEED();
}

TEST_F(ClientTest, ConnectWithScheme) {
    EXPECT_TRUE(client.connect("https://localhost:443"));
}

TEST_F(ClientTest, ConnectWithoutScheme) {
    // Should auto-prepend https://
    EXPECT_TRUE(client.connect("localhost:443"));
}

TEST_F(ClientTest, ConnectStripsTrailingSlash) {
    EXPECT_TRUE(client.connect("https://localhost:443/"));
}

TEST_F(ClientTest, SetVerbose) {
    // Should not throw or crash
    client.set_verbose(true);
    client.set_verbose(false);
    SUCCEED();
}

TEST_F(ClientTest, SetInsecure) {
    client.set_insecure(true);
    client.set_insecure(false);
    SUCCEED();
}

TEST_F(ClientTest, SetCaCert) {
    client.set_ca_cert("/path/to/ca.pem");
    SUCCEED();
}

TEST_F(ClientTest, SetBandwidthLimit) {
    client.set_bandwidth_limit(1024 * 1024, 512 * 1024); // 1MB/s up, 512KB/s down
    SUCCEED();
}

TEST_F(ClientTest, SetProgressCallback) {
    bool called = false;
    client.set_progress_callback([&called](const std::string&, size_t, size_t) {
        called = true;
    });
    SUCCEED(); // Just ensure it compiles and doesn't crash
}

TEST_F(ClientTest, AuthenticateNonexistentCert) {
    EXPECT_FALSE(client.authenticate("/nonexistent/path/cert.pem"));
}

TEST_F(ClientTest, AuthenticateExistingFile) {
    // Create a temporary file
    std::string tmp_path = "/tmp/quicftp_test_cert.pem";
    {
        std::ofstream f(tmp_path);
        f << "fake cert data";
    }
    EXPECT_TRUE(client.authenticate(tmp_path));
    std::filesystem::remove(tmp_path);
}

// ============================================================================
// Upload/Download without connection (should report NotConnected)
// ============================================================================

TEST_F(ClientTest, UploadWithoutConnect) {
    // Create a temp file
    std::string tmp_path = "/tmp/quicftp_test_upload.txt";
    {
        std::ofstream f(tmp_path);
        f << "test data";
    }

    auto result = client.upload(tmp_path, "remote.txt");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::NotConnected);
    EXPECT_FALSE(result.message.empty());

    std::filesystem::remove(tmp_path);
}

TEST_F(ClientTest, DownloadWithoutConnect) {
    auto result = client.download("remote.txt", "/tmp/quicftp_test_dl.txt");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::NotConnected);
}

// ============================================================================
// File error tests
// ============================================================================

TEST_F(ClientTest, UploadNonexistentFile) {
    client.connect("https://localhost:443");

    auto result = client.upload("/nonexistent/path/file.txt", "remote.txt");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::FileOpenError);
}

TEST_F(ClientTest, DownloadToInvalidPath) {
    client.connect("https://localhost:443");

    auto result = client.download("remote.txt", "/nonexistent/dir/output.txt");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ErrorCode::FileOpenError);
}

// ============================================================================
// Legacy bool API
// ============================================================================

TEST_F(ClientTest, LegacyUploadWithoutConnect) {
    std::string tmp_path = "/tmp/quicftp_test_legacy.txt";
    {
        std::ofstream f(tmp_path);
        f << "test data";
    }

    EXPECT_FALSE(client.upload_file(tmp_path, "remote.txt"));
    std::filesystem::remove(tmp_path);
}

TEST_F(ClientTest, LegacyDownloadWithoutConnect) {
    EXPECT_FALSE(client.download_file("remote.txt", "/tmp/quicftp_test_legacy_dl.txt"));
}

// ============================================================================
// Batch operations without connection
// ============================================================================

TEST_F(ClientTest, BatchUploadEmpty) {
    client.connect("https://localhost:443");
    auto result = client.upload_batch({});
    EXPECT_TRUE(result.all_success);
    EXPECT_EQ(result.results.size(), 0u);
}

TEST_F(ClientTest, BatchDownloadEmpty) {
    client.connect("https://localhost:443");
    auto result = client.download_batch({});
    EXPECT_TRUE(result.all_success);
    EXPECT_EQ(result.results.size(), 0u);
}

TEST_F(ClientTest, BatchUploadWithBadFiles) {
    client.connect("https://localhost:443");

    std::vector<std::pair<std::string, std::string>> files = {
        {"/nonexistent/file1.txt", "remote1.txt"},
        {"/nonexistent/file2.txt", "remote2.txt"},
    };

    auto result = client.upload_batch(files);
    EXPECT_FALSE(result.all_success);
    EXPECT_EQ(result.failed_count, 2u);
    EXPECT_EQ(result.succeeded_count, 0u);
}
