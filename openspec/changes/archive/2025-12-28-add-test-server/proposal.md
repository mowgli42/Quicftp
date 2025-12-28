# Change: Add Basic Test Server with Verbose Information

## Why
The project currently only has a client implementation. A test server is needed to:
- Enable end-to-end testing of the client library
- Provide development and debugging capabilities
- Support integration testing scenarios
- Offer verbose logging for troubleshooting connection and transfer issues

## What Changes
- Add a new test server implementation (`quicftp_server` or similar)
- Server SHALL provide verbose output for all operations (connections, authentication, file transfers)
- Server SHALL support the same operations as the client (upload, download, authentication)
- Server SHALL be suitable for local testing and development

## Impact
- Affected specs: New capability `test-server`
- Affected code: New server implementation files
- No breaking changes to existing client code

