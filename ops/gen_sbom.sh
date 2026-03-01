#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="artifacts/sbom"
mkdir -p "${OUT_DIR}"

if command -v syft >/dev/null 2>&1; then
  syft dir:. -o cyclonedx-json="${OUT_DIR}/sbom.cdx.json"
  syft dir:. -o spdx-json="${OUT_DIR}/sbom.spdx.json"
  echo "SBOM generated via local syft."
  exit 0
fi

if command -v docker >/dev/null 2>&1; then
  docker run --rm -v "$(pwd):/workspace" anchore/syft:latest dir:/workspace -o cyclonedx-json >"${OUT_DIR}/sbom.cdx.json"
  docker run --rm -v "$(pwd):/workspace" anchore/syft:latest dir:/workspace -o spdx-json >"${OUT_DIR}/sbom.spdx.json"
  echo "SBOM generated via dockerized syft."
  exit 0
fi

echo "syft (or docker) not found; SBOM generation skipped." >&2
exit 1
