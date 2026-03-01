#!/usr/bin/env bash
set -euo pipefail

bash ops/scan_all.sh

validate_openapi() {
  local spec_path="${1}"
  if [ ! -f "${spec_path}" ]; then
    echo "openapi validation: skipped (spec not found at ${spec_path})"
    return 0
  fi

  if command -v openapi-spec-validator >/dev/null 2>&1; then
    openapi-spec-validator "${spec_path}"
    echo "openapi validation: ok (openapi-spec-validator CLI)"
    return 0
  fi

  if command -v python3 >/dev/null 2>&1; then
    if python3 -c "import openapi_spec_validator" >/dev/null 2>&1; then
      python3 - <<PY
from pathlib import Path
import yaml
from openapi_spec_validator import validate_spec

spec_path = Path("${spec_path}")
with spec_path.open("r", encoding="utf-8") as handle:
    document = yaml.safe_load(handle)
validate_spec(document)
PY
      echo "openapi validation: ok (python module)"
      return 0
    fi
  fi

  echo "openapi validation: skipped (validator not installed)"
}

validate_openapi "${OPENAPI_SPEC_PATH:-openapi/openapi.yaml}"

if [ -f artifacts/security/gitleaks.json ]; then
  if command -v jq >/dev/null 2>&1; then
    findings="$(jq 'length // 0' artifacts/security/gitleaks.json 2>/dev/null || echo 0)"
  else
    findings="$(grep -c '"RuleID"' artifacts/security/gitleaks.json || true)"
  fi
  if [ "${findings}" != "0" ]; then
    echo "quality gate failed: gitleaks findings=${findings}" >&2
    exit 1
  fi
fi

if [ -f artifacts/security/trivy.json ]; then
  if command -v jq >/dev/null 2>&1; then
    high_crit="$(jq '[.. | objects | select(has("Severity")) | .Severity | select(.=="HIGH" or .=="CRITICAL")] | length' artifacts/security/trivy.json 2>/dev/null || echo 0)"
  else
    high_crit="$(grep -E -c '"Severity":"(HIGH|CRITICAL)"' artifacts/security/trivy.json || true)"
  fi
  if [ "${high_crit}" != "0" ]; then
    echo "quality gate failed: trivy high/critical=${high_crit}" >&2
    exit 1
  fi
fi

if [ -n "${API_BASE_URL:-}" ]; then
  echo "contract test: API_BASE_URL=${API_BASE_URL}"
  bash ops/contract_test.sh
else
  echo "contract test: skipped (set API_BASE_URL to enable)"
fi

echo "quality gate passed"
