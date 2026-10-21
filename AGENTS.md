<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

# Quicftp

C++ client library + CLI for file transfer over QUIC (HTTP/3) using libcurl + Caddy Docker server.
Stack: C++17, CMake, libcurl (HTTP/3), Docker/Caddy, OpenSpec, Beads.
Posture: ponytail (repo >30 days).
Shared health pack in `.cursor/skills/` + `.cursor/rules/`.

## Commands

- Build: `mkdir -p build && cd build && cmake -DCMAKE_CXX_COMPILER=g++ .. && make`
- Unit tests: `cd build && cmake -DBUILD_TESTING=ON .. && make && ctest --output-on-failure`
- Integration: `docker build -t quicftp-caddy ./docker/caddy && bash scripts/integration_test.sh`
- Optional HTTP/3 deps: `./scripts/build_curl_http3.sh`
- Secrets: `bash scripts/scan-secrets.sh .`
- Beads: `bd ready` / `bd show <id>` / `bd sync`

## Hard prohibitions

- Do not commit private keys, `*-key.pem`, `*.key`, or `BEGIN … PRIVATE KEY`. Generate with `certs/generate_certs.sh`; keys stay gitignored. Public certs OK.
- Do not reintroduce custom ngtcp2 QUIC stack. Use libcurl + Caddy.
- Do not invent CLI flags, CMake options, or routes not in the tree.
- Do not rewrite OpenSpec / Beads to match aspirational future. Update only when code changed.

## Verify by change type

| Change | Check |
| --- | --- |
| Client / C++ | cmake + unit tests in `build/` |
| Server / Docker | integration_test.sh with Caddy |
| TLS / certs | `bash scripts/scan-secrets.sh .` |
| Spec | matching OpenSpec + beads |

## Source of truth

- Behavior: `openspec/` + README usage examples
- Remaining work: Beads (`.beads/`) / GitHub issues
- Health bar: `.cursor/rules/repo-health.mdc`

## House vocabulary

- quicftpclient — CLI binary. Do not say "ftp client" generically.
- Parallel transfers — curl_multi batch. Do not invent custom queues.
- Phase 2 — production-ready features (TLS, errors, progress, bandwidth) already done.

## Borrowed patterns

- Hard prohibitions + verification matrix from ossrules.md (hard-prohibition / verification-matrix patterns).
- Single source of truth (OpenSpec/Beads) — do not duplicate specs.
