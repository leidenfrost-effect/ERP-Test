#!/usr/bin/env bash
set -euo pipefail

SPEC_PATH="${OPENAPI_SPEC_PATH:-openapi/openapi.yaml}"
API_BASE_URL="${API_BASE_URL:-http://127.0.0.1:18080}"
SCHEMATHESIS_FULL="${SCHEMATHESIS_FULL:-0}"

if [ ! -f "${SPEC_PATH}" ]; then
  echo "contract test skipped: spec not found (${SPEC_PATH})"
  exit 0
fi

if ! command -v schemathesis >/dev/null 2>&1; then
  echo "contract test skipped: schemathesis is not installed"
  exit 0
fi

args=(
  run
  "${SPEC_PATH}"
  --url "${API_BASE_URL}"
  --max-failures=1
)

if [ "${SCHEMATHESIS_FULL}" != "1" ]; then
  # Default smoke scope keeps the contract check stable for local runs without auth fixtures.
  args+=(--include-path-regex '^/(health|ready|auth/status)$')
fi

if [ -n "${API_AUTH_TOKEN:-}" ]; then
  args+=(--header "Authorization: Bearer ${API_AUTH_TOKEN}")
fi

schemathesis "${args[@]}"
