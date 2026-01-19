FROM caddy:2.7-builder AS builder

RUN xcaddy build \
    --with github.com/mholt/caddy-webdav

FROM caddy:2.7-alpine

COPY --from=builder /usr/bin/caddy /usr/bin/caddy

# Create a directory for file uploads
RUN mkdir -p /data/files && \
    chown -R caddy:caddy /data/files

WORKDIR /data
