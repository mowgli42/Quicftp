# Change: Phase 2 — Production-Ready Features

## Why
The Quicftp project completed its Phase 1 architecture refactor (custom ngtcp2 stack → Caddy + libcurl), establishing a solid foundation. However, the project is not yet production-ready. It lacks:
- **Testing infrastructure**: No unit tests, no integration tests, no CI/CD
- **Security hardening**: TLS verification is disabled, no CA cert management
- **Error handling**: All operations return `bool` with no structured error details
- **Operational features**: No progress reporting, no resume support, no bandwidth control
- **Developer experience**: CLI has no proper argument parsing, no help output, no config files

Phase 2 addresses these gaps to make Quicftp reliable, secure, and usable in production environments.

## What Changes

### Security (P0 — Critical)
- **BREAKING**: Enable TLS certificate verification by default (`CURLOPT_SSL_VERIFYPEER=1L`)
- Add CA certificate configuration support (custom CA bundles)
- Add `--insecure` flag for development-only usage
- Remove hardcoded `SSL_VERIFYPEER=0L` and `SSL_VERIFYHOST=0L`

### Testing (P1 — High)
- Add unit test suite for `quicftp::Client` using GoogleTest
- Add Docker Compose integration test suite (client → Caddy server)
- Add GitHub Actions CI/CD pipeline (build + test + lint)

### Error Handling (P1 — High)
- Replace `bool` return types with structured `Result<T>` or error enum
- Add error codes and detailed error messages with context
- Add error callback support for async error handling

### Operational Features (P2 — Medium)
- Wire up `ProgressCallback` to libcurl's progress mechanism
- Add resumable transfers using HTTP Range headers and `CURLOPT_RESUME_FROM_LARGE`
- Add connection pool and TLS session caching for faster reconnects
- Add CLI enhancements: proper argument parsing, `--progress`, `--resume`, `--ca-cert`, `--help`

### Performance (P3 — Low)
- Add bandwidth limiting via `CURLOPT_MAX_SEND_SPEED_LARGE`/`CURLOPT_MAX_RECV_SPEED_LARGE`

## Impact
- Affected specs: `file-transfer` (modified), `test-server` (modified)
- Affected code: `quicftp_client.h`, `quicftp_client.cc`, `quicftpclient-cli.cc`, `CMakeLists.txt`
- New files: `tests/`, `docker-compose.yml`, `.github/workflows/ci.yml`
- New dependencies: GoogleTest (test framework), Docker Compose (integration tests)
- **BREAKING**: TLS verification enabled by default may break existing setups using self-signed certs without CA config

## Beads Tracking
All Phase 2 tasks are tracked in Beads under epic `workspace-n49` ("Phase 2: Production-Ready Features") with the following priority order:
1. **P0**: TLS certificate verification (workspace-tl8)
2. **P1**: Unit tests (workspace-5p6) → Integration tests (workspace-k7h) → CI/CD (workspace-4am)
3. **P1**: Structured error handling (workspace-zaq)
4. **P2**: Progress reporting (workspace-0xh), resumable transfers (workspace-ch9), connection pool (workspace-p7f), CLI enhancements (workspace-z05)
5. **P3**: Bandwidth limiting (workspace-6k8)
