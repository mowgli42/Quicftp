# Design: Containerized QUIC Architecture

## Context
The project needs a reliable way to transfer files over QUIC without maintaining a custom QUIC stack implementation.

## Architecture Components

### 1. The Server (Caddy Container)
We will use **Caddy** with the `webdav` module (or standard PUT support) running in a Docker container.

-   **Image**: `caddy:alpine` (custom build with `http.handlers.webdav` if needed, or just use a simple Go sidecar if Caddy WebDAV is too limited).
-   **Configuration (`Caddyfile`)**:
    ```caddyfile
    {
        servers :443 {
            protocol {
                experimental_http3
            }
        }
    }

    :443 {
        tls internal # Self-signed for internal overlay network
        
        # Enable WebDAV for uploads
        route {
            webdav {
                root /data
                prefix /files
            }
        }
    }
    ```
-   **Pros**:
    -   HTTP/3 is "free".
    -   TLS is handled automatically.
    -   Filesystem management is robust.

### 2. The Client (C++ + libcurl)
The client library `quicftp::Client` will be a wrapper around `libcurl`.

-   **Dependency**: `libcurl` built with `USE_NGTCP2=ON` and `USE_NGHTTP3=ON`.
-   **Implementation**:
    -   `upload_file(local, remote)` -> `PUT https://server/files/remote` (HTTP/3).
    -   `download_file(remote, local)` -> `GET https://server/files/remote` (HTTP/3).
-   **Parallelism**: `curl_multi_init` interface allows efficient parallel transfers on a single thread (or pool).

### 3. Connectivity (Overlay)
-   **ZeroTier / Tailscale**: Installed on the host or as a sidecar container.
-   **Function**: Provides a stable IP (e.g., `10.147.20.1`) that is reachable through NATs.
-   **Security**: Mutual authentication and encryption provided by the overlay layer (WireGuard/Salsa20).

## Alternatives Considered

### Custom Go/Python Server
-   **Idea**: Write a small `quic-go` or `aioquic` script.
-   **Verdict**: Valid backup if Caddy configuration proves annoying, but Caddy is "infrastructure as code" which is cleaner.

### MsQuic / mvfst
-   **Idea**: Use a different C++ QUIC library.
-   **Verdict**: Still requires writing the "Server" logic. Using a pre-packaged Server Container is much faster.

## Testing Strategy
**Docker Compose** is the source of truth.

```yaml
services:
  caddy:
    build: ./docker/caddy
    volumes:
      - ./test_data:/data
    ports:
      - "443:443/udp"
    networks:
      - overlay_sim

  client_test:
    build: .
    command: ./build/test_suite
    networks:
      - overlay_sim

networks:
  overlay_sim:
    driver: bridge
```
