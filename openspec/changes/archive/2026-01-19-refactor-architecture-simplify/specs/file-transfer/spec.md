## RENAMED Requirements
- FROM: `### Requirement: Stream Multiplexing`
- TO: `### Requirement: Parallel File Transfers`

## MODIFIED Requirements
### Requirement: Parallel File Transfers
The system SHALL support transferring multiple files simultaneously.

#### Scenario: Parallel file uploads
- **WHEN** a client uploads multiple files simultaneously
- **THEN** files SHALL be transferred concurrently
- **THEN** failure in one transfer SHALL NOT stop others

#### Scenario: Parallel file downloads
- **WHEN** a client downloads multiple files simultaneously
- **THEN** files SHALL be transferred concurrently
- **THEN** the client SHALL be able to track progress of each transfer independently

### Requirement: Stream Prioritization
The system SHALL support prioritization of file transfers.

#### Scenario: Priority-based transfer order
- **WHEN** multiple files are queued for transfer with different priorities
- **THEN** higher priority files SHALL receive more bandwidth allocation
- **THEN** the system SHALL allow users to specify transfer priorities

### Requirement: Flow Control
The system SHALL implement proper flow control.

#### Scenario: Connection-level flow control
- **WHEN** the total connection bandwidth is limited
- **THEN** the system SHALL ensure fair bandwidth distribution across transfers
- **THEN** the system SHALL prevent buffer overflow conditions

### Requirement: Congestion Control
The system SHALL utilize congestion control mechanisms for adaptive transfer rates.

#### Scenario: Adaptive bandwidth usage
- **WHEN** network conditions change (increased latency, packet loss)
- **THEN** the system SHALL automatically reduce transfer rates
- **THEN** when network conditions improve, transfer rates SHALL increase

## REMOVED Requirements
### Requirement: Zero-Round-Trip Time (0-RTT) Connection Establishment
**Reason**: This is a specific QUIC optimization. The new architecture (Overlay+TCP) relies on the overlay for connection management.
**Migration**: N/A

### Requirement: Connection Migration
**Reason**: Handled transparently by the Overlay network (ZeroTier/Tailscale).
**Migration**: N/A

### Requirement: Path MTU Discovery
**Reason**: Handled by the underlying TCP/IP stack and Overlay network.
**Migration**: N/A

## ADDED Requirements
### Requirement: Overlay Network Integration
The system SHALL operate over a secure overlay network to ensure connectivity and security.

#### Scenario: NAT Traversal
- **WHEN** peers are behind different NATs/Firewalls
- **THEN** they SHALL be able to connect via the overlay IP
- **THEN** no manual port forwarding SHALL be required

#### Scenario: Network Security
- **WHEN** data is transferred between peers
- **THEN** it SHALL be encrypted by the overlay network
- **THEN** peers SHALL be authenticated via the overlay mechanism

### Requirement: Out-of-Band Signaling
The system SHALL use a separate control plane for signaling and coordination.

#### Scenario: Transfer Negotiation
- **WHEN** a peer wants to send a file
- **THEN** it SHALL send a request via the signaling channel (e.g., NATS)
- **THEN** the receiver SHALL accept or reject the request before data transfer begins
