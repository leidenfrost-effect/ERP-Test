#!/usr/bin/env bash
set -u

ARTIFACT_DIR="artifacts/security"
mkdir -p "${ARTIFACT_DIR}"

semgrep_status=0
gitleaks_status=0
trivy_status=0
osv_status=0

write_skipped() {
  local path="$1"
  local reason="$2"
  printf '{"skipped":true,"reason":"%s"}\n' "${reason}" >"${path}"
}

if command -v semgrep >/dev/null 2>&1; then
  if semgrep scan --config=auto --json . >"${ARTIFACT_DIR}/semgrep.json" 2>"${ARTIFACT_DIR}/semgrep.json.stderr"; then
    echo "semgrep: ok"
  else
    semgrep_status=$?
    echo "semgrep: failed (${semgrep_status})"
  fi
else
  write_skipped "${ARTIFACT_DIR}/semgrep.json" "tool_not_found"
  echo "semgrep: skipped (command not found)"
fi

if command -v gitleaks >/dev/null 2>&1; then
  if gitleaks detect --redact --report-format json --report-path "${ARTIFACT_DIR}/gitleaks.json" >"${ARTIFACT_DIR}/gitleaks.json.stdout" 2>"${ARTIFACT_DIR}/gitleaks.json.stderr"; then
    echo "gitleaks: ok"
  else
    gitleaks_status=$?
    echo "gitleaks: findings_or_error (${gitleaks_status})"
    if [ ! -s "${ARTIFACT_DIR}/gitleaks.json" ]; then
      printf '[]\n' >"${ARTIFACT_DIR}/gitleaks.json"
    fi
  fi
else
  write_skipped "${ARTIFACT_DIR}/gitleaks.json" "tool_not_found"
  echo "gitleaks: skipped (command not found)"
fi

if command -v trivy >/dev/null 2>&1; then
  if trivy fs --scanners vuln,secret --format json . >"${ARTIFACT_DIR}/trivy.json" 2>"${ARTIFACT_DIR}/trivy.json.stderr"; then
    echo "trivy: ok"
  else
    trivy_status=$?
    echo "trivy: failed (${trivy_status})"
  fi
else
  write_skipped "${ARTIFACT_DIR}/trivy.json" "tool_not_found"
  echo "trivy: skipped (command not found)"
fi

if command -v osv-scanner >/dev/null 2>&1; then
  if osv-scanner -r . --json >"${ARTIFACT_DIR}/osv.json" 2>"${ARTIFACT_DIR}/osv.json.stderr"; then
    echo "osv-scanner: ok"
  else
    osv_status=$?
    echo "osv-scanner: failed (${osv_status})"
  fi
else
  write_skipped "${ARTIFACT_DIR}/osv.json" "tool_not_found"
  echo "osv-scanner: skipped (command not found)"
fi

semgrep_findings=0
gitleaks_findings=0
trivy_findings=0
trivy_high_critical=0
osv_findings=0

if command -v jq >/dev/null 2>&1; then
  semgrep_findings="$(jq '.results | length // 0' "${ARTIFACT_DIR}/semgrep.json" 2>/dev/null || echo 0)"
  gitleaks_findings="$(jq 'length // 0' "${ARTIFACT_DIR}/gitleaks.json" 2>/dev/null || echo 0)"
  trivy_findings="$(jq '[.. | objects | select(has("Severity"))] | length' "${ARTIFACT_DIR}/trivy.json" 2>/dev/null || echo 0)"
  trivy_high_critical="$(jq '[.. | objects | select(has("Severity")) | .Severity | select(.=="HIGH" or .=="CRITICAL")] | length' "${ARTIFACT_DIR}/trivy.json" 2>/dev/null || echo 0)"
  osv_findings="$(jq '[.. | objects | select(has("vulnerabilities")) | .vulnerabilities[]?] | length' "${ARTIFACT_DIR}/osv.json" 2>/dev/null || echo 0)"
fi

cat >"${ARTIFACT_DIR}/summary.md" <<EOF
# Security Scan Summary

| Tool | Exit status | Finding count |
| --- | ---: | ---: |
| semgrep | ${semgrep_status} | ${semgrep_findings} |
| gitleaks | ${gitleaks_status} | ${gitleaks_findings} |
| trivy (all severities) | ${trivy_status} | ${trivy_findings} |
| trivy (high+critical) | ${trivy_status} | ${trivy_high_critical} |
| osv-scanner | ${osv_status} | ${osv_findings} |

Notes:
- Missing tools are emitted as \`{"skipped":true}\` JSON files.
- \`ops/quality_gate.sh\` fails on gitleaks findings and trivy high/critical findings by default.
EOF

echo "security scan artifacts created under ${ARTIFACT_DIR}"
if [ "${semgrep_status}" -ne 0 ] || [ "${trivy_status}" -ne 0 ] || [ "${osv_status}" -ne 0 ]; then
  echo "one or more scanners failed; inspect artifacts/security/*.stderr"
fi
