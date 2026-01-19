# Implementation Tasks: Containerized QUIC

## 1. Server Setup (Docker)
- [ ] 1.1 **Research Caddy WebDAV**:
  - Verify Caddy's `webdav` module supports robust PUT uploads.
  - Alternative: Simple Go `quic-go` HTTP/3 server that saves files from PUT body.
- [ ] 1.2 **Create Dockerfile**:
  - Create `docker/server/Dockerfile`.
  - Configure `Caddyfile` for HTTP/3 and self-signed TLS.
- [ ] 1.3 **Test Manual Upload**:
  - Run container.
  - Use `curl --http3 -T file.txt https://localhost/...` to verify upload works.

## 2. Client Dependency (libcurl)
- [ ] 2.1 **Build Infrastructure**:
  - specific instructions to build `libcurl` with `ngtcp2` + `nghttp3` + `openssl`.
  - Create a script `scripts/build_curl_http3.sh` or use `vcpkg`.
- [ ] 2.2 **Integration**:
  - Update `CMakeLists.txt` to link against the custom libcurl.

## 3. Client Implementation
- [ ] 3.1 **Refactor `quicftp::Client`**:
  - Replace `impl_` with a libcurl-based implementation.
  - Implement `upload_file` using `CURLOPT_UPLOAD`.
  - Implement `download_file` using standard GET.
- [ ] 3.2 **Parallel Transfers**:
  - Implement `upload_files` using `curl_multi` interface for multiplexing.

## 4. Testing & Cleanup
- [ ] 4.1 **Docker Compose Suite**:
  - Create `docker-compose.yml` orchestrating the Client Test Container and Server Container.
- [ ] 4.2 **Delete Legacy Code**:
  - Remove `quic_wrapper.*`, `ngtcp2` direct dependencies (except what curl needs internally).
- [ ] 4.3 **Documentation**:
  - Update `README.md` to explain the Container + Overlay architecture.
