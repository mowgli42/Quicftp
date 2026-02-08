## ADDED Requirements

### Requirement: TLS Certificate Verification
The client SHALL verify server TLS certificates by default and provide configuration for custom Certificate Authority (CA) bundles.

#### Scenario: Default secure connection
- **WHEN** a client connects to a server without explicit TLS configuration
- **THEN** the client SHALL verify the server's TLS certificate against system CA certificates
- **THEN** the connection SHALL fail if the certificate is invalid, expired, or untrusted

#### Scenario: Custom CA certificate
- **WHEN** a client configures a custom CA certificate path
- **THEN** the client SHALL use the specified CA bundle for certificate verification
- **THEN** self-signed certificates SHALL be accepted if their CA is in the bundle

#### Scenario: Development insecure mode
- **WHEN** a client explicitly enables insecure mode
- **THEN** TLS certificate verification SHALL be disabled
- **THEN** the client SHALL log a warning about insecure operation

### Requirement: Structured Error Reporting
The client API SHALL return structured error information for all transfer operations instead of simple boolean success/failure.

#### Scenario: Transfer failure with details
- **WHEN** a file transfer operation fails
- **THEN** the result SHALL include an error code, human-readable message, and HTTP status code (if applicable)
- **THEN** the error code SHALL distinguish between connection errors, authentication errors, file errors, and server errors

#### Scenario: Parallel transfer per-file results
- **WHEN** a parallel transfer operation completes (partially or fully)
- **THEN** the result SHALL include individual status for each file in the batch
- **THEN** each file result SHALL indicate success or failure with specific error details

### Requirement: Transfer Progress Tracking
The client SHALL provide real-time progress information for active file transfers.

#### Scenario: Single file progress
- **WHEN** a file transfer is in progress
- **THEN** the progress callback SHALL receive the file path, bytes transferred, and total bytes
- **THEN** progress updates SHALL be emitted at regular intervals during the transfer

#### Scenario: Parallel transfer progress
- **WHEN** multiple files are transferring in parallel
- **THEN** progress callbacks SHALL identify which file the update pertains to
- **THEN** each file SHALL report its progress independently

### Requirement: Resumable File Transfers
The client SHALL support resuming interrupted file transfers from the point of interruption.

#### Scenario: Resume interrupted download
- **WHEN** a download was interrupted and the partial file exists locally
- **THEN** the client SHALL use HTTP Range headers to request only the remaining bytes
- **THEN** the download SHALL continue from the interrupted position

#### Scenario: Server resume support detection
- **WHEN** the client attempts to resume a transfer
- **THEN** the client SHALL verify the server supports Range requests
- **THEN** if the server does not support resume, the client SHALL restart the full transfer

### Requirement: Connection Session Reuse
The client SHALL support connection pooling and TLS session caching for faster subsequent connections.

#### Scenario: TLS session caching
- **WHEN** a client reconnects to a previously connected server
- **THEN** the client SHALL attempt to reuse the cached TLS session
- **THEN** session reuse SHALL reduce connection establishment time

#### Scenario: Connection keep-alive
- **WHEN** multiple sequential transfers are performed to the same server
- **THEN** the client SHALL reuse the existing connection rather than establishing a new one
- **THEN** connection reuse SHALL be transparent to the caller

### Requirement: Transfer Bandwidth Limiting
The client SHALL support configurable bandwidth limits for upload and download operations.

#### Scenario: Upload bandwidth limit
- **WHEN** an upload bandwidth limit is configured
- **THEN** the client SHALL not exceed the specified upload rate in bytes per second
- **THEN** the limit SHALL apply to both single and parallel transfers

#### Scenario: Download bandwidth limit
- **WHEN** a download bandwidth limit is configured
- **THEN** the client SHALL not exceed the specified download rate in bytes per second
- **THEN** the limit SHALL apply to both single and parallel transfers
