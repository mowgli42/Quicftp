# Implementation Tasks: Phase 2 — Production-Ready Features

## 1. Security Hardening (P0)
- [ ] 1.1 Enable `CURLOPT_SSL_VERIFYPEER=1L` and `CURLOPT_SSL_VERIFYHOST=2L` by default
- [ ] 1.2 Add `set_ca_cert(const std::string& ca_path)` method to Client API
- [ ] 1.3 Add `set_insecure(bool insecure)` method for dev-only usage
- [ ] 1.4 Update `configure_handle()` to use CA cert when configured
- [ ] 1.5 Update CLI to accept `--ca-cert` and `--insecure` flags
- [ ] 1.6 Update Caddy Docker setup to generate proper CA certificates for testing

## 2. Structured Error Handling (P1)
- [ ] 2.1 Define `TransferError` enum (ConnectionFailed, AuthFailed, FileNotFound, UploadFailed, DownloadFailed, Timeout, etc.)
- [ ] 2.2 Define `Result<T>` template or `TransferResult` struct with error code, message, and HTTP status
- [ ] 2.3 Refactor `upload_file` and `download_file` to return `TransferResult`
- [ ] 2.4 Refactor `upload_files` and `download_files` to return per-file results
- [ ] 2.5 Add `set_error_callback()` for async error notification
- [ ] 2.6 Update CLI to display structured error information

## 3. Unit Tests (P1)
- [ ] 3.1 Add GoogleTest dependency to CMakeLists.txt (FetchContent)
- [ ] 3.2 Create `tests/test_client.cc` with tests for connect(), authenticate(), set_verbose()
- [ ] 3.3 Create mock/stub HTTP server for unit tests (or use libcurl test utilities)
- [ ] 3.4 Add upload/download error handling tests
- [ ] 3.5 Add parallel transfer tests
- [ ] 3.6 Add `ctest` integration to CMakeLists.txt

## 4. Integration Tests (P1)
- [ ] 4.1 Create `docker-compose.yml` with Caddy server and test runner services
- [ ] 4.2 Create test scripts for upload/download scenarios
- [ ] 4.3 Add parallel transfer integration tests
- [ ] 4.4 Add large file transfer tests (verify streaming, no memory issues)
- [ ] 4.5 Add error scenario tests (invalid cert, server down, file not found)

## 5. CI/CD Pipeline (P1)
- [ ] 5.1 Create `.github/workflows/ci.yml` with build matrix (Ubuntu, macOS)
- [ ] 5.2 Add libcurl HTTP/3 build step (cache deps for speed)
- [ ] 5.3 Add unit test execution step
- [ ] 5.4 Add Docker-based integration test step
- [ ] 5.5 Add linting step (clang-tidy or cppcheck)

## 6. Transfer Progress Reporting (P2)
- [ ] 6.1 Implement `CURLOPT_XFERINFOFUNCTION` callback in `configure_handle()`
- [ ] 6.2 Wire progress data to existing `ProgressCallback` in Client
- [ ] 6.3 Add per-file progress tracking for `upload_files`/`download_files`
- [ ] 6.4 Add `--progress` flag to CLI with terminal progress bar

## 7. Resumable Transfers (P2)
- [ ] 7.1 Add `CURLOPT_RESUME_FROM_LARGE` support for downloads
- [ ] 7.2 Detect partial files and auto-resume on retry
- [ ] 7.3 Add `--resume` CLI flag
- [ ] 7.4 Handle server-side Range header support detection

## 8. Connection Pool & Session Reuse (P2)
- [ ] 8.1 Create shared `CURLSH` handle for TLS session caching
- [ ] 8.2 Implement connection keep-alive between sequential transfers
- [ ] 8.3 Add session ticket caching for 0-RTT-like reconnects
- [ ] 8.4 Benchmark connection reuse vs fresh connections

## 9. CLI Enhancements (P2)
- [ ] 9.1 Add proper argument parsing (getopt_long or CLI11 library)
- [ ] 9.2 Add `--help` and `--version` flags
- [ ] 9.3 Add `--progress`, `--resume`, `--bandwidth-limit`, `--ca-cert`, `--insecure` flags
- [ ] 9.4 Add configuration file support (~/.quicftprc or similar)
- [ ] 9.5 Improve error output with color and structured messages

## 10. Bandwidth Limiting (P3)
- [ ] 10.1 Add `CURLOPT_MAX_SEND_SPEED_LARGE` and `CURLOPT_MAX_RECV_SPEED_LARGE` support
- [ ] 10.2 Add `set_bandwidth_limit(size_t upload_bps, size_t download_bps)` to Client API
- [ ] 10.3 Add `--bandwidth-limit` flag to CLI
