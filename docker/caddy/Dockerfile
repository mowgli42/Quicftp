FROM caddy:2.9-builder AS builder

RUN xcaddy build \
    --with github.com/mholt/caddy-webdav

FROM caddy:2.9-alpine

COPY --from=builder /usr/bin/caddy /usr/bin/caddy

# Create a directory for file uploads
RUN mkdir -p /data/files && \
    chmod 777 /data/files

COPY Caddyfile /etc/caddy/Caddyfile

WORKDIR /data
