#!/bin/bash
# Integration test script for quicftp
# Requires: Docker running, project built (./build/quicftpclient)
set -e

PASS=0
FAIL=0
# Resolve absolute path to client binary
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CLIENT="${SCRIPT_DIR}/build/quicftpclient"
SERVER_NAME="quicftp-test-server"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

pass() { echo -e "${GREEN}PASS${NC}: $1"; PASS=$((PASS + 1)); }
fail() { echo -e "${RED}FAIL${NC}: $1"; FAIL=$((FAIL + 1)); }

echo "=== Quicftp Integration Tests ==="
echo ""

# Setup: create temp working directory and start server
WORKDIR=$(mktemp -d)
echo "Working directory: $WORKDIR"
echo "Starting test server..."
docker rm -f "$SERVER_NAME" 2>/dev/null || true
docker run -d -p 443:443/udp -p 443:443/tcp -p 80:80 --name "$SERVER_NAME" quicftp-caddy > /dev/null 2>&1
sleep 3

cleanup() {
    echo ""
    echo "Cleaning up..."
    docker rm -f "$SERVER_NAME" 2>/dev/null || true
    rm -rf "$WORKDIR" 2>/dev/null || true
    echo ""
    echo "=== Results: $PASS passed, $FAIL failed ==="
    [ "$FAIL" -eq 0 ] && exit 0 || exit 1
}
trap cleanup EXIT

cd "$WORKDIR"

# Test 1: Single upload
echo "Test 1: Single file upload"
echo "test content" > upload_test.txt
if $CLIENT localhost:443 upload upload_test.txt 2>&1 | grep -q "Uploaded"; then
    pass "Single file upload"
else
    fail "Single file upload"
fi

# Test 2: Single download
echo "Test 2: Single file download"
rm -f upload_test.txt
if $CLIENT localhost:443 download upload_test.txt 2>&1 | grep -q "Downloaded"; then
    if [ "$(cat upload_test.txt)" = "test content" ]; then
        pass "Single file download (content verified)"
    else
        fail "Single file download (content mismatch)"
    fi
else
    fail "Single file download"
fi

# Test 3: Parallel upload
echo "Test 3: Parallel upload"
echo "alpha" > t_a.txt
echo "bravo" > t_b.txt
echo "charlie" > t_c.txt
if $CLIENT localhost:443 upload t_a.txt t_b.txt t_c.txt 2>&1 | grep -q "3/3"; then
    pass "Parallel upload (3 files)"
else
    fail "Parallel upload"
fi

# Test 4: Parallel download
echo "Test 4: Parallel download"
rm -f t_a.txt t_b.txt t_c.txt
if $CLIENT localhost:443 download t_a.txt t_b.txt t_c.txt 2>&1 | grep -q "3/3"; then
    pass "Parallel download (3 files)"
else
    fail "Parallel download"
fi

# Test 5: Large file checksum
echo "Test 5: Large file transfer (5MB)"
dd if=/dev/urandom of=large_test.bin bs=1M count=5 2>/dev/null
MD5_ORIG=$(md5sum large_test.bin | cut -d' ' -f1)
$CLIENT localhost:443 upload large_test.bin > /dev/null 2>&1
mv large_test.bin large_test_orig.bin
$CLIENT localhost:443 download large_test.bin > /dev/null 2>&1
MD5_DL=$(md5sum large_test.bin | cut -d' ' -f1)
if [ "$MD5_ORIG" = "$MD5_DL" ]; then
    pass "Large file transfer (checksum: $MD5_ORIG)"
else
    fail "Large file transfer (checksum mismatch: $MD5_ORIG vs $MD5_DL)"
fi

# Test 6: 404 error handling
echo "Test 6: Error handling (404)"
OUTPUT=$($CLIENT localhost:443 download nonexistent_xyz_test.bin 2>&1) || true
if echo "$OUTPUT" | grep -q "404"; then
    if [ ! -f nonexistent_xyz_test.bin ]; then
        pass "404 error handling (no partial file)"
    else
        fail "404 error handling (partial file created)"
        rm -f nonexistent_xyz_test.bin
    fi
else
    fail "404 error handling (no 404 in output)"
fi

# Test 7: CLI --help
echo "Test 7: CLI --help"
if $CLIENT --help 2>&1 | grep -q "Usage"; then
    pass "CLI --help"
else
    fail "CLI --help"
fi

# Test 8: CLI --version
echo "Test 8: CLI --version"
if $CLIENT --version 2>&1 | grep -q "0.2.0"; then
    pass "CLI --version"
else
    fail "CLI --version"
fi
