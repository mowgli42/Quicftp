# Test certificates

Private keys are **not** stored in git.

Generate a local CA plus server/client keys:

```bash
./certs/generate_certs.sh
```

That writes `ca-key.pem`, `server-key.pem`, and `client-key.pem` next to this file. Those files are gitignored.

These certs are for local development only (`CN=localhost`). Do not reuse them in any real environment. If keys were ever committed, treat them as compromised and regenerate.