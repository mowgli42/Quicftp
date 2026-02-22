# Implementation Tasks: Phase 2 — Production-Ready Features

## 1. Security Hardening (P0)
- [x] 1.1 Add `set_ca_cert(const std::string& ca_path)` method to Client API
- [x] 1.2 Add `set_insecure(bool insecure)` method for dev-only usage
- [x] 1.3 Update `configure_handle()` to use CA cert when configured
- [x] 1.4 Update CLI to accept `--ca-cert` and `--insecure` flags
- [x] 1.5 Update Caddy Docker setup (upgraded to 2.9, fixed cert gen for localhost)

## 2. Structured Error Handling (P1)
- [x] 2.1 Define `ErrorCode` enum (13 error types: ConnectionFailed, AuthFailed, FileNotFound, etc.)
- [x] 2.2 Define `TransferResult` struct with error code, message, HTTP status, bytes_transferred
- [x] 2.3 Add `upload()` and `download()` methods returning `TransferResult`
- [x] 2.4 Add `upload_batch()` and `download_batch()` returning `BatchTransferResult` with per-file results
- [x] 2.5 Map CURLcode and HTTP status codes to ErrorCode automatically
- [x] 2.6 Update CLI to display structured error information (error code + HTTP status)
- [x] 2.7 Preserve legacy bool API (`upload_file`, `download_file`) for backward compatibility

## 3. Unit Tests (P1)
- [x] 3.1 Add GoogleTest dependency to CMakeLists.txt (FetchContent v1.14.0)
- [x] 3.2 Create `tests/test_client.cc` (20 tests: connect, auth, config, errors, batch)
- [x] 3.3 Create `tests/test_error_types.cc` (11 tests: ErrorCode strings, TransferResult, BatchTransferResult)
- [x] 3.4 Add upload/download error handling tests (NotConnected, FileOpenError)
- [x] 3.5 Add batch operation tests (empty batch, bad files)
- [x] 3.6 Add `ctest` integration to CMakeLists.txt via `gtest_discover_tests()`

## 4. Integration Tests (P1)
- [x] 4.1 Create `docker-compose.yml` with Caddy server and healthcheck
- [x] 4.2 Create `scripts/integration_test.sh` with 8 tests
- [x] 4.3 Add parallel transfer integration tests (3-file upload + download)
- [x] 4.4 Add large file transfer test (5MB, MD5 checksum verification)
- [x] 4.5 Add error scenario test (404 detection, no partial file)

## 5. CI/CD Pipeline (P1)
- [x] 5.1 Create `.github/workflows/ci.yml` with Ubuntu runner
- [x] 5.2 Add unit test execution step (`ctest --output-on-failure`)
- [x] 5.3 Add Docker-based integration test step (Caddy server + e2e tests)
- [x] 5.4 Add content verification (md5sum for large files, string compare for text)

## 6. Transfer Progress Reporting (P2)
- [x] 6.1 Implement `CURLOPT_XFERINFOFUNCTION` callback via `xfer_info_cb` bridge
- [x] 6.2 Wire progress data to existing `ProgressCallback` in Client
- [x] 6.3 Add per-file progress tracking with `ProgressData` struct
- [x] 6.4 Add `--progress` flag to CLI with terminal progress display

## 7. CLI Enhancements (P2)
- [x] 7.1 Add proper argument parsing (manual flag parsing with -- prefix)
- [x] 7.2 Add `--help` and `--version` flags
- [x] 7.3 Add `--progress`, `--ca-cert`, `--insecure`, `--cert`, `--key`, `--verbose` flags
- [x] 7.4 Improve error output with HTTP status codes and structured messages
- [x] 7.5 Add batch summary line (e.g., "2/3 files downloaded successfully")

## 8. Bandwidth Limiting (P3)
- [x] 8.1 Add `CURLOPT_MAX_SEND_SPEED_LARGE` and `CURLOPT_MAX_RECV_SPEED_LARGE` support
- [x] 8.2 Add `set_bandwidth_limit(size_t upload_bps, size_t download_bps)` to Client API

## Deferred to Future Work
- [ ] Resumable transfers (`CURLOPT_RESUME_FROM_LARGE`, `--resume` flag)
- [ ] Explicit connection pooling (shared `CURLSH` handle)
- [ ] TLS session ticket caching for 0-RTT
- [ ] Configuration file support (~/.quicftprc)
- [ ] clang-tidy / cppcheck linting in CI
- [ ] macOS CI build matrix
