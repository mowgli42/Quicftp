## Context
Quicftp completed Phase 1 (architecture refactor to Caddy + libcurl). The codebase is functional but lacks testing, security hardening, proper error handling, and production-grade features. Phase 2 brings the project to production readiness.

## Goals / Non-Goals
- **Goals**:
  - Make the client library production-safe (TLS verification, structured errors)
  - Establish testing infrastructure (unit + integration + CI)
  - Add operational features (progress, resume, bandwidth control)
  - Improve developer experience (CLI enhancements, better errors)

- **Non-Goals**:
  - Change the overall architecture (Caddy server + libcurl client stays)
  - Add new transport protocols beyond HTTP/3
  - Build a GUI or web interface
  - Support server-side development (Caddy handles that)

## Decisions

### Error Handling Strategy
- **Decision**: Use a `TransferResult` struct with error enum + message, not exceptions
- **Why**: C++ exceptions are disabled in many embedded/performance-critical contexts. Error structs are explicit and composable.
- **Alternative**: Use `std::expected<T, E>` (C++23) — rejected because project targets C++17

### Test Framework
- **Decision**: GoogleTest via CMake FetchContent
- **Why**: Industry standard for C++, excellent CMake integration, no system install needed
- **Alternative**: Catch2 — good but less common in CI/CD tooling

### CLI Argument Parsing
- **Decision**: Use CLI11 header-only library or `getopt_long`
- **Why**: CLI11 is modern, header-only, and integrates well with C++17. `getopt_long` is zero-dependency.
- **Decision deferred** until implementation; either is acceptable

### TLS Verification
- **Decision**: Enable by default, add `--insecure` flag for dev
- **Why**: Security-by-default is the correct posture. Breaking change is acceptable at this project stage.
- **Migration**: Existing users must either configure CA certs or use `--insecure` explicitly

## Risks / Trade-offs
- **Risk**: Enabling TLS verification breaks existing dev setups → Mitigation: `--insecure` flag, clear migration docs
- **Risk**: HTTP/3 libcurl build complexity in CI → Mitigation: Cache built deps, provide pre-built Docker image
- **Risk**: GoogleTest adds build time → Mitigation: Only build tests when `BUILD_TESTING=ON`

## Migration Plan
Phase 2 changes are additive except for TLS verification (breaking):
1. Add tests first (no behavior change)
2. Add error handling (backward-compatible API additions first)
3. Enable TLS verification (breaking — major version bump)
4. Add operational features (all additive)

## Open Questions
- Should we support C++20 `std::expected` as an optional error type?
- Should the connection pool be bounded or unbounded?
- What is the target test coverage percentage?
