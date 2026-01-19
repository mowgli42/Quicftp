# Change: Refactor Architecture for Simplification

## Why
The current custom `ngtcp2` implementation is overly complex and blocking progress.
- **Maintenance**: Managing raw QUIC handshakes and TLS state is error-prone.
- **Testing**: Simulating firewalls for raw UDP is difficult.
- **Goal**: The user wants to retain QUIC for transfer but move to a more manageable architecture using existing server codebases.

## What Changes
We propose replacing the custom QUIC stack with industry-standard tools:

1.  **Server: Containerized Caddy (HTTP/3)**
    -   Instead of a custom C++ server, we will deploy **Caddy** in a Docker container.
    -   **Why Caddy?** It has native, production-grade HTTP/3 (QUIC) support powered by `quic-go`. It is easy to configure and secure by default.
    -   **Role**: Accepts file uploads (PUT) and serves downloads (GET) over QUIC.

2.  **Client: libcurl (with HTTP/3)**
    -   Refactor the C++ `quicftp::Client` to use **libcurl** configured with HTTP/3 support (via `ngtcp2` backend).
    -   **Why libcurl?** It abstracts the entire handshake, stream management, and connection logic. We simply say `curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3)`.

3.  **Connectivity: Overlay Network (ZeroTier/Tailscale)**
    -   Use an overlay network to provide a secure, flat addressing space.
    -   **Benefit**: Resolves NAT traversal and firewall issues transparently. The app connects to the "Server IP" provided by the overlay.

4.  **Control Plane: NATS (Optional)**
    -   Retain NATS for out-of-band signaling (service discovery, transfer coordination) if complex orchestration is needed.

## Impact
-   **Code Removal**: Massive deletion of `quic_wrapper.cc`, `quic_wrapper.h`, and `quicftp_server.cc`.
-   **New Dependencies**:
    -   **Server**: Docker, Caddy image.
    -   **Client**: `libcurl` (compiled with HTTP/3 support).
-   **Testing**:
    -   Testing becomes "Client vs Caddy Container" in Docker Compose.

## Risks
-   **Build Complexity**: Building `libcurl` with HTTP/3 support (ngtcp2+nghttp3) is non-trivial but well-documented. We will need a robust CMake setup or vcpkg manifest.
