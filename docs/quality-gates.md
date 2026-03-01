# Quality Gates

## Local

Run:

```bash
bash ops/quality_gate.sh
```

This wrapper:

- runs `ops/scan_all.sh`
- validates `openapi/openapi.yaml` when validator tooling exists
- fails on any gitleaks finding
- fails on trivy `HIGH` / `CRITICAL` findings
- optionally runs contract tests when `API_BASE_URL` is set

## CI Recommendation

Use the same fail criteria in CI and upload:

- `artifacts/security/*.json`
- `artifacts/security/summary.md`
- `artifacts/sbom/*.json`

## Contract Test Toggle

Contract tests are disabled by default in `quality_gate.sh` unless `API_BASE_URL` is set:

```bash
API_BASE_URL="http://127.0.0.1:18080" bash ops/quality_gate.sh
```

Use full schemathesis scope only when auth test credentials are available:

```bash
API_BASE_URL="http://127.0.0.1:18080" \
API_AUTH_TOKEN="change-me" \
SCHEMATHESIS_FULL=1 \
bash ops/contract_test.sh
```
