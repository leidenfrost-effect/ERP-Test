# API Contract Workflow

## Files

- Source contract: `openapi/openapi.yaml`
- Contract test helper: `ops/contract_test.sh`
- Typed client generator helper: `ops/gen_client.sh`

## Validate and Test

Validate quality gate (includes OpenAPI validation when validator exists):

```bash
bash ops/quality_gate.sh
```

Contract smoke test against running API:

```bash
API_BASE_URL="http://127.0.0.1:18080" bash ops/contract_test.sh
```

Run full schemathesis scope (requires auth for protected endpoints):

```bash
API_BASE_URL="http://127.0.0.1:18080" \
API_AUTH_TOKEN="change-me" \
SCHEMATHESIS_FULL=1 \
bash ops/contract_test.sh
```

## Generate Mobile Client

Default generator is `dart`.

```bash
bash ops/gen_client.sh
```

Custom generator/output:

```bash
OPENAPI_CLIENT_GENERATOR=typescript-fetch \
OPENAPI_CLIENT_OUT_DIR=clients/web_api_client \
bash ops/gen_client.sh
```
